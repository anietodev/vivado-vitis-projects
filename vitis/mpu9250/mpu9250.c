/**
 * @file mpu9250.c
 * @author  Andrea Nieto Gil
 * @date    2025
 *
 * @brief Interfaz I2C para el sensor inercial MPU9250 usando AXI IIC en Zybo Z7-10.
 *
 * Este fichero implementa:
 *   - Inicialización del MPU6050 + magnetómetro AK8963
 *   - Lectura del acelerómetro, giroscopio y temperatura
 *   - Lectura del magnetómetro con verificación de DRDY y overflow
 *   - Conversión de datos a unidades físicas (g, ºC, dps, microTesla)
 *
 * El código está diseñado para ejecutarse en entorno bare-metal con Vitis 2019.2.
 * Utiliza el controlador AXI IIC de Xilinx para comunicación con el sensor.
 *
 * @note Requiere habilitar soporte de float en printf desde el BSP:
 *       - enable_printf_float = true
 */

#include "xparameters.h"
#include "xiic.h"
#include "sleep.h"
#include "platform.h"
#include "xil_printf.h"
#include <stdio.h>
#include <stdint.h>

/* -------------------- Direcciones I2C -------------------- */
#define MPU_ADDR   0x68   /**< Dirección I2C del MPU6050/9250 */
#define MAG_ADDR   0x0C   /**< Dirección I2C del magnetómetro AK8963 */

/* -------------------- Sensibilidades --------------------- */
#define ACCEL_SENS 4096.0f   /**< Factor de conversión para ±8g */
#define GYRO_SENS    16.4f   /**< Factor de conversión para ±2000 dps */

/* -------------------- Variables globales ------------------ */
static XIic Iic;           /**< Instancia del controlador AXI IIC */
static uint8_t ASA[3];     /**< Ajustes de sensibilidad del magnetómetro */



/* ============================================================
 *                     DECLARACION DE FUNCIONES
 * ============================================================*/

/** Funciones I2C */
int IIC_WriteReg(uint8_t DevAddr, uint8_t RegAddr, uint8_t Data);
int IIC_ReadReg(uint8_t DevAddr, uint8_t RegAddr, uint8_t *buf, int len);

/** Inicialización */
void init_mpu9250(void);

/** Lectura sensores */
void read_mpu9250(void);
void read_magnetometer(void);



/* ============================================================
 *                        MAIN
 * ============================================================*/

int main()
{
    init_platform();

    print("Iniciando MPU9250...\n\r");
    init_mpu9250();

    while (1) {
        read_mpu9250();
        read_magnetometer();
        usleep(1000000); /* 1 Hz */
    }

    cleanup_platform();
    return 0;
}



/* ============================================================
 *                         FUNCIONES
 * ============================================================*/

/**
 * @brief Escribe un registro de un dispositivo I2C.
 *
 * @param DevAddr Dirección I2C del dispositivo.
 * @param RegAddr Dirección del registro a escribir.
 * @param Data    Byte a escribir.
 * @return Número de bytes enviados o error.
 */
int IIC_WriteReg(uint8_t DevAddr, uint8_t RegAddr, uint8_t Data)
{
    uint8_t buffer[2] = {RegAddr, Data};
    return XIic_Send(XPAR_AXI_IIC_MPU9250_BASEADDR, DevAddr, buffer, 2, XIIC_STOP);
}

/**
 * @brief Lee uno o varios registros usando repeated start.
 *
 * @param DevAddr Dirección I2C del dispositivo.
 * @param RegAddr Registro inicial a leer.
 * @param buf     Buffer donde almacenar los datos.
 * @param len     Cantidad de bytes a leer.
 * @return Número de bytes leídos o error.
 */
int IIC_ReadReg(uint8_t DevAddr, uint8_t RegAddr, uint8_t *buf, int len)
{
    int status;

    status = XIic_Send(
        XPAR_AXI_IIC_MPU9250_BASEADDR,
        DevAddr,
        &RegAddr,
        1,
        XIIC_REPEATED_START
    );

    if (status != 1)
        return status;

    return XIic_Recv(
        XPAR_AXI_IIC_MPU9250_BASEADDR,
        DevAddr,
        buf,
        len,
        XIIC_STOP
    );
}

/* ============================================================
 *                INICIALIZACIÓN MPU9250 / AK8963
 * ============================================================*/

/**
 * @brief Inicializa el MPU9250 y el magnetómetro AK8963.
 *
 * Configura:
 *  - Acelerómetro en ±8g
 *  - Giroscopio en ±2000 dps
 *  - DLPF y sample rate
 *  - Bypass I2C para acceder al AK8963
 *  - Lectura de factores ASA del magnetómetro
 */
void init_mpu9250()
{
    int Status = XIic_Initialize(&Iic, XPAR_AXI_IIC_MPU9250_DEVICE_ID);

    if (Status != XST_SUCCESS) {
        print("Error inicializando IIC\r\n");
        return;
    }

    XIic_Start(&Iic);

    /* Despertar MPU */
    IIC_WriteReg(MPU_ADDR, 0x6B, 0x00);
    usleep(1000);

    /* Configuración MPU */
    IIC_WriteReg(MPU_ADDR, 0x1A, 0x03);
    IIC_WriteReg(MPU_ADDR, 0x1B, 0x18);
    IIC_WriteReg(MPU_ADDR, 0x1C, 0x10);
    IIC_WriteReg(MPU_ADDR, 0x1D, 0x03);
    IIC_WriteReg(MPU_ADDR, 0x19, 0x07);

    /* Habilitar bypass para el magnetómetro */
    IIC_WriteReg(MPU_ADDR, 0x37, 0x02);
    usleep(1000);

    /* Inicialización AK8963 */
    IIC_WriteReg(MAG_ADDR, 0x0A, 0x00);
    usleep(1000);

    IIC_WriteReg(MAG_ADDR, 0x0A, 0x0F); /* Modo fuse-ROM */
    usleep(1000);

    IIC_ReadReg(MAG_ADDR, 0x10, ASA, 3);

    IIC_WriteReg(MAG_ADDR, 0x0A, 0x00);
    usleep(1000);

    /* WHO_AM_I */
    uint8_t who = 0;
    IIC_ReadReg(MAG_ADDR, 0x00, &who, 1);
    xil_printf("AK8963 WHO_AM_I = 0x%02X\r\n", who);

    /* Configurar magnetómetro: 16 bits, 100 Hz */
    IIC_WriteReg(MAG_ADDR, 0x0A, 0x16);

    /* Confirmar ASA */
    IIC_ReadReg(MAG_ADDR, 0x10, ASA, 3);
    xil_printf("ASA: %d %d %d\r\n", ASA[0], ASA[1], ASA[2]);
}

/* ============================================================
 *                 LECTURA: MPU (Accel, Gyro, Temp)
 * ============================================================*/

/**
 * @brief Lee acelerómetro, giroscopio y temperatura del MPU9250.
 *
 * Convierte los valores a:
 *  - Aceleración en 'g'
 *  - Temperatura en °C
 *  - Velocidad angular en dps
 */
void read_mpu9250()
{
    uint8_t data[14];

    if (IIC_ReadReg(MPU_ADDR, 0x3B, data, 14) != 14) {
        print("Error leyendo datos del MPU\r\n");
        return;
    }

    int16_t ax = (data[0] << 8) | data[1];
    int16_t ay = (data[2] << 8) | data[3];
    int16_t az = (data[4] << 8) | data[5];
    int16_t temp = (data[6] << 8) | data[7];
    int16_t gx = (data[8] << 8) | data[9];
    int16_t gy = (data[10] << 8) | data[11];
    int16_t gz = (data[12] << 8) | data[13];

    float ax_g = ax / ACCEL_SENS;
    float ay_g = ay / ACCEL_SENS;
    float az_g = az / ACCEL_SENS;

    float temp_c = temp / 340.0f + 36.53f;

    float gx_dps = gx / GYRO_SENS;
    float gy_dps = gy / GYRO_SENS;
    float gz_dps = gz / GYRO_SENS;

    printf("Acel (g): %.3f, %.3f, %.3f | Temp %.2f C | "
           "Giro (dps): %.3f %.3f %.3f\r\n",
           ax_g, ay_g, az_g, temp_c, gx_dps, gy_dps, gz_dps);
}

/* ============================================================
 *                 LECTURA: Magnetómetro AK8963
 * ============================================================*/

/**
 * @brief Lee el magnetómetro AK8963 comprobando DRDY y overflow.
 *
 * Convierte datos a microTesla usando los ajustes ASA.
 */
void read_magnetometer()
{
    uint8_t st1;
    IIC_ReadReg(MAG_ADDR, 0x02, &st1, 1);

    if (!(st1 & 0x01))
        return; /* No hay datos nuevos */

    uint8_t data[7];

    if (IIC_ReadReg(MAG_ADDR, 0x03, data, 7) != 7) {
        print("Error leyendo datos del magnetometro\r\n");
        return;
    }

    if (data[6] & 0x08)
        return; /* Overflow */

    int16_t mx = (data[1] << 8) | data[0];
    int16_t my = (data[3] << 8) | data[2];
    int16_t mz = (data[5] << 8) | data[4];

    float adjX = ((ASA[0] - 128) / 256.0f) + 1.0f;
    float adjY = ((ASA[1] - 128) / 256.0f) + 1.0f;
    float adjZ = ((ASA[2] - 128) / 256.0f) + 1.0f;

    float mx_uT = mx * adjX * 0.15f;
    float my_uT = my * adjY * 0.15f;
    float mz_uT = mz * adjZ * 0.15f;

    printf("Mag (uT): %.2f, %.2f, %.2f\r\n", mx_uT, my_uT, mz_uT);
}
