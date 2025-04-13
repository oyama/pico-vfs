/*
 * Copyright 2024, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <errno.h>
#include <hardware/adc.h>
#include <hardware/rtc.h>
#include <lwip/altcp_tls.h>
#include <lwip/apps/mqtt.h>
#include <lwip/dns.h>
#include <pico/cyw43_arch.h>
#include <pico/stdlib.h>
#include <stdio.h>

#define MQTT_SERVER             "io.adafruit.com"  // See https://io.adafruit.com
#define MQTT_TOPIC              (MQTT_USER "/feeds/temperature")
#define MQTT_QOS_AT_LEAST_ONCE  1
#define LOCAL_QUEUE_PATH        "/temperature.txt"

extern bool ntp_sync(void);
extern const char *get_timestamp(void);

static bool network_init(ip_addr_t *mqtt_server) {
    if (cyw43_arch_init()) {
        printf("Wi-Fi init failed");
        return false;
    }
    cyw43_arch_enable_sta_mode();

    while (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 10000) != 0) {
        printf("Wi-Fi connection failed\n");
        sleep_ms(1000);
    }
    printf("Wi-Fi connect ok\n");

    while (!ntp_sync()) {
        printf("NTP sync failed\n");
        sleep_ms(1000);
    }
    printf("NTP sync ok\n");

    while (1) {
        cyw43_arch_lwip_begin();
        int err = dns_gethostbyname(MQTT_SERVER, mqtt_server, NULL, NULL);
        cyw43_arch_lwip_end();
        if (err == ERR_OK) {
            while (mqtt_server->addr == 0)
                sleep_ms(1);
            break;
        }
        printf("lookup %s failed=%d\n", MQTT_SERVER, err);
        sleep_ms(1000);
    }
    printf("lookup %s ok\n", MQTT_SERVER);
    return true;
}

static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
    (void)client;
    (void)arg;
    if (status == MQTT_CONNECT_ACCEPTED) {
        printf("MQTT connect ok\n");
    } else {
        printf("MQTT connect failed: status=%d\n", status);
    }
}

static bool mantain_network_connection(mqtt_client_t *client, ip_addr_t *server) {
    bool wifi_status = netif_is_up(&cyw43_state.netif[0]) && netif_is_link_up(&cyw43_state.netif[0]);
    if (!wifi_status) {

        cyw43_arch_lwip_begin();
        cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
        int err = cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 1000);
        cyw43_arch_lwip_end();

        if (err != 0) {
            printf("Wi-Fi connection failed: err=%d\n", err);
            return false;
        }
        printf("Wi-Fi connect ok\n");
    }
    if (!mqtt_client_is_connected(client)) {
        static struct mqtt_connect_client_info_t ci = {0};
        ci.client_id = "";
        ci.client_user = MQTT_USER;
        ci.client_pass = MQTT_PASSWORD;
        ci.keep_alive = 30;
        if (ci.tls_config == NULL)
            ci.tls_config = altcp_tls_create_config_client(NULL, 0);
        if (ci.tls_config == NULL) {
            printf("mantain_network_connection: altcp_tls_create_config_client failed\n");
            return false;
        }

        cyw43_arch_lwip_begin();
        mqtt_client_connect(client, server, MQTT_TLS_PORT, mqtt_connection_cb, 0, &ci);
        cyw43_arch_lwip_end();
        return true;
    }
    return true;
}

// Report on-board thermometer data
static float read_sensor_data() {
    uint16_t raw = adc_read();
    const float conversion_factor = 3.3f / (1 << 12);
    float voltage = raw * conversion_factor;
    float temperature = 27.0f - (voltage - 0.706f) / 0.001721f;
    return temperature;
}

static void publish_message(mqtt_client_t *client, float data, const char *timestamp) {
    (void)client;
    char payload[100] = {0};
    snprintf(payload, sizeof(payload), "{\"timestamp\":\"%s\", \"value\":%.2f}", timestamp, data);

    cyw43_arch_lwip_begin();
    mqtt_publish(client, MQTT_TOPIC, payload, strlen(payload), MQTT_QOS_AT_LEAST_ONCE, 0, NULL, NULL);
    cyw43_arch_lwip_end();

    printf("Publish: %s\n", payload);
}

static void publish_message_from_file(mqtt_client_t *client) {
    FILE *fp = fopen(LOCAL_QUEUE_PATH, "r");
    if (fp == NULL) {
        return;
    }

    char timestamp[30] = {0};
    float data = 0;
    while (fscanf(fp, "%[^,],%f\n", timestamp, &data) != EOF) {
        publish_message(client, data, timestamp);
    }
    fclose(fp);
    remove(LOCAL_QUEUE_PATH);
}

static void save_data_to_file(float data, const char *timestamp) {
    FILE *fp = fopen(LOCAL_QUEUE_PATH, "a");
    if (fp == NULL) {
        printf("fopen failed: %s\n", strerror(errno));
        return;
    }
    fprintf(fp, "%s,%.2f\n", timestamp, data);
    printf("Queue: %s,%.2f\n", timestamp, data);
    fclose(fp);
}

int main(void) {
    stdio_init_all();
    rtc_init();
    adc_init();
    adc_set_temp_sensor_enabled(true);
    adc_select_input(4);

    ip_addr_t mqtt_server_addr = {0};
    if (!network_init(&mqtt_server_addr)) {
        printf("network_init failed\n");
    }
    mqtt_client_t *client = mqtt_client_new();
    while (1) {
        float sensor_value = read_sensor_data();
        const char *timestamp = get_timestamp();
        if (mantain_network_connection(client, &mqtt_server_addr)) {
            // Normal operation
            publish_message_from_file(client);
            publish_message(client, sensor_value, timestamp);
        } else {
            // Network outage operation
            save_data_to_file(sensor_value, timestamp);
        }
        sleep_ms(10 * 1000);
    }

    mqtt_client_free(client);
    cyw43_arch_deinit();
    return 0;
}
