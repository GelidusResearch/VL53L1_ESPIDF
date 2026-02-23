/**
 * i2c_platform_esp.c
 *
 * I2C device interface for the
 * Espressif Internet-of-Things (IoT) Development Framework ESP-IDF
 *
 * I2C_Master is the global I2C interface shared by all devices
 *
 * (c) 2021 by David Asher
 * https://github.com/david-asher
 * https://www.linkedin.com/in/davidasher/
 * This code is licensed under MIT license, see LICENSE.txt for details
 */

#include "i2c_platform_esp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    i2c_port_t port;
    gpio_num_t pin_sda;
    gpio_num_t pin_scl;
    uint32_t freq;
    uint8_t dev_address;
    i2c_rw_t read_write;
    i2c_master_bus_handle_t bus_handle;
    i2c_master_dev_handle_t device_handles[128];
    uint8_t *write_buffer;
    size_t write_len;
    size_t write_cap;
    uint8_t *read_buffer;
    size_t read_len;
    bool transaction_open;
} I2C_Master_t;

I2C_Master_t *I2C_Master = (I2C_Master_t *) NULL;

static bool i2c_ensure_write_capacity(size_t needed)
{
    if (I2C_Master->write_cap >= needed) {
        return true;
    }
    size_t new_cap = I2C_Master->write_cap == 0 ? 16 : I2C_Master->write_cap;
    while (new_cap < needed) {
        new_cap *= 2;
    }
    uint8_t *new_buffer = (uint8_t *) realloc(I2C_Master->write_buffer, new_cap);
    if (new_buffer == NULL) {
        return false;
    }
    I2C_Master->write_buffer = new_buffer;
    I2C_Master->write_cap = new_cap;
    return true;
}

static i2c_master_dev_handle_t i2c_get_or_create_device(uint8_t address)
{
    if (address >= 128 || I2C_Master == NULL || I2C_Master->bus_handle == NULL) {
        return NULL;
    }
    if (I2C_Master->device_handles[address] != NULL) {
        return I2C_Master->device_handles[address];
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = I2C_Master->freq,
        .scl_wait_us = 0,
        .flags.disable_ack_check = 0,
    };

    i2c_master_dev_handle_t handle = NULL;
    esp_err_t err = i2c_master_bus_add_device(I2C_Master->bus_handle, &dev_cfg, &handle);
    if (err != ESP_OK) {
        return NULL;
    }
    I2C_Master->device_handles[address] = handle;
    return handle;
}

void i2c_scan()
{
    printf("\r\nI2C device scan: ");
    if (I2C_Master == NULL || I2C_Master->bus_handle == NULL) {
        i2c_init();
    }
    for (uint8_t i = 1; i < 127; i++)
    {
        esp_err_t ret = i2c_master_probe(I2C_Master->bus_handle, i, 100);
        if (ret != ESP_OK) continue;
        printf("0x%02X | ", i );
    }
    printf( "\r\n" );
}

I2C_Master_t *i2c_master_setup()
{
    I2C_Master_t *new_master = (I2C_Master_t *) malloc( sizeof( I2C_Master_t ) );
    memset(new_master, 0, sizeof(I2C_Master_t));
    new_master->port = I2C_DEFAULT_PORT;
    new_master->pin_sda = I2C_DEFAULT_SDA;
    new_master->pin_scl = I2C_DEFAULT_SCL;
    new_master->freq = I2C_DEFAULT_FREQ;
    new_master->dev_address = I2C_NO_DEVICE;
    new_master->read_write = I2C_WRITE;
    new_master->bus_handle = NULL;
    new_master->write_buffer = NULL;
    new_master->write_len = 0;
    new_master->write_cap = 0;
    new_master->read_buffer = NULL;
    new_master->read_len = 0;
    new_master->transaction_open = false;
    return new_master;
}

void i2c_get_config( i2c_port_t *port, gpio_num_t *pin_sda, gpio_num_t *pin_scl, uint32_t *freq )
{
    *port = I2C_Master->port;
    *pin_sda = I2C_Master->pin_sda;
    *pin_scl = I2C_Master->pin_scl;
    *freq = I2C_Master->freq;
}

void i2c_init_driver()
{
    i2c_master_bus_config_t conf = {
        .i2c_port = I2C_Master->port,
        .sda_io_num = I2C_Master->pin_sda,
        .scl_io_num = I2C_Master->pin_scl,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 4,
        .flags.enable_internal_pullup = 1,
        .flags.allow_pd = 0,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&conf, &I2C_Master->bus_handle));
}

void i2c_init_config( i2c_port_t port, gpio_num_t pin_sda, gpio_num_t pin_scl, uint32_t freq )
{
    if ( I2C_Master != (I2C_Master_t *) NULL ) return;
    I2C_Master = i2c_master_setup();
    I2C_Master->port = port;
    I2C_Master->pin_sda = pin_sda;
    I2C_Master->pin_scl = pin_scl;
    I2C_Master->freq = freq;
    i2c_init_driver();
}

void i2c_init()
{
    if ( I2C_Master != (I2C_Master_t *) NULL ) return;
    I2C_Master = i2c_master_setup();
    i2c_init_driver();
}

void i2c_remove()
{
    if (I2C_Master == NULL) {
        return;
    }
    for (int i = 0; i < 128; i++) {
        if (I2C_Master->device_handles[i] != NULL) {
            ESP_ERROR_CHECK(i2c_master_bus_rm_device(I2C_Master->device_handles[i]));
            I2C_Master->device_handles[i] = NULL;
        }
    }
    if (I2C_Master->bus_handle != NULL) {
        ESP_ERROR_CHECK(i2c_del_master_bus(I2C_Master->bus_handle));
        I2C_Master->bus_handle = NULL;
    }
    free(I2C_Master->write_buffer);
    free( I2C_Master );
    I2C_Master = (I2C_Master_t *) NULL;
}

void i2c_upgrade( uint32_t upgrade_freq )
{
    i2c_port_t port;
    gpio_num_t pin_sda;
    gpio_num_t pin_scl;
    uint32_t   old_freq;

    i2c_get_config( &port, &pin_sda, &pin_scl, &old_freq );
    i2c_remove();
    i2c_init_config( port, pin_sda, pin_scl, upgrade_freq );
}

bool i2c_start( uint8_t i2c_device_address, i2c_rw_t read_write )
{
    if (I2C_Master == NULL) {
        i2c_init();
    }
    if (!I2C_Master->transaction_open) {
        I2C_Master->transaction_open = true;
        I2C_Master->write_len = 0;
        I2C_Master->read_buffer = NULL;
        I2C_Master->read_len = 0;
        I2C_Master->dev_address = i2c_device_address;
    } else if (I2C_Master->dev_address != i2c_device_address) {
        return false;
    }
    I2C_Master->read_write = read_write;
    if (i2c_get_or_create_device(i2c_device_address) == NULL) {
        return false;
    }
    return true;
}

size_t i2c_write_byte( uint8_t data_byte_out )
{
    if ( I2C_Master == NULL || !I2C_Master->transaction_open || I2C_Master->read_write != I2C_WRITE ) return 0;
    if (!i2c_ensure_write_capacity(I2C_Master->write_len + 1)) {
        return 0;
    }
    I2C_Master->write_buffer[I2C_Master->write_len++] = data_byte_out;
    return 1;
}

size_t i2c_write( uint8_t *pByteBuffer, size_t NumByteToWrite )
{
    if ( I2C_Master == NULL || !I2C_Master->transaction_open || I2C_Master->read_write != I2C_WRITE ) return 0;
    if (!i2c_ensure_write_capacity(I2C_Master->write_len + NumByteToWrite)) {
        return 0;
    }
    memcpy(&I2C_Master->write_buffer[I2C_Master->write_len], pByteBuffer, NumByteToWrite);
    I2C_Master->write_len += NumByteToWrite;
    return NumByteToWrite;
}

uint8_t i2c_read_byte()
{
    uint8_t byteBuffer = 0;
    if (i2c_read(&byteBuffer, 1) != 1) {
        return 0;
    }
    return byteBuffer;
}

size_t i2c_read( uint8_t *pByteBuffer, size_t NumByteToRead )
{
    if ( I2C_Master == NULL || !I2C_Master->transaction_open ) return 0;
    I2C_Master->read_write = I2C_READ;
    I2C_Master->read_buffer = pByteBuffer;
    I2C_Master->read_len = NumByteToRead;
    return NumByteToRead;
}

esp_err_t i2c_transmit()
{
    if (I2C_Master == NULL || !I2C_Master->transaction_open || I2C_Master->dev_address == I2C_NO_DEVICE) {
        return ESP_ERR_INVALID_STATE;
    }

    i2c_master_dev_handle_t dev = i2c_get_or_create_device(I2C_Master->dev_address);
    if (dev == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t i2c_error;
    if (I2C_Master->read_len > 0) {
        if (I2C_Master->write_len > 0) {
            i2c_error = i2c_master_transmit_receive(dev,
                                                    I2C_Master->write_buffer,
                                                    I2C_Master->write_len,
                                                    I2C_Master->read_buffer,
                                                    I2C_Master->read_len,
                                                    1000);
        } else {
            i2c_error = i2c_master_receive(dev,
                                           I2C_Master->read_buffer,
                                           I2C_Master->read_len,
                                           1000);
        }
    } else {
        i2c_error = i2c_master_transmit(dev,
                                        I2C_Master->write_buffer,
                                        I2C_Master->write_len,
                                        1000);
    }

    I2C_Master->dev_address = I2C_NO_DEVICE;
    I2C_Master->read_write = I2C_WRITE;
    I2C_Master->read_buffer = NULL;
    I2C_Master->read_len = 0;
    I2C_Master->write_len = 0;
    I2C_Master->transaction_open = false;
    return i2c_error;
}
