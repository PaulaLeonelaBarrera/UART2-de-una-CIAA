/*  UART + INTERRUPCIÓN - EJ. 5) 
    Miembros:
    Barrera, Paula
    Herrera, Cesar
*/

#include "stdlib.h"
#include "stdint.h"
#include "configuracion.h"
#include "led.h"
#include <string.h>

#define USB_UART   LPC_USART2
#define MODEM_UART LPC_USART3

/* Mensajes */
#define OVERRUN  "ERROR OVERRUN\r\n"
#define PARITY   "ERROR PARITY\r\n"
#define FRAMING  "ERROR FRAMING\r\n"
#define BREAK    "ERROR BREAK\r\n"
#define UNKNOWN  "ERROR UNKNOWN\r\n"

static uint8_t actualizar = 0;

/* VARIABLES PARA UART2 (interrupción) */
volatile uint8_t uart2_byte = 0;
volatile uint8_t uart2_error = 0;
volatile uint8_t uart2_flag  = 0;

/* ───────────────────────────────────────── */
void SysTick_Handler(void)
{
    static int contador = 0;

    contador++;
    if (contador % 500 == 0) {
        Led_Toggle(RED_LED);
    }
    if (contador % 1000 == 0) {
        contador = 0;
        actualizar = 1;
    }
}

/* ───────────────────────────────────────── */
/*     CONFIGURACIÓN UART (IGUAL AL LAB)     */
/* ───────────────────────────────────────── */
void ConfigurarUART(LPC_USART_T * pUART)
{
    Chip_UART_Init(pUART);

    /* Habilito DLAB */
    pUART->LCR |= (1 << 7);

    /* Divisor para baudrate */
    pUART->DLL = 0x4C;
    pUART->DLM = 0x01;

    /* Deshabilito DLAB */
    pUART->LCR &= ~(1 << 7);

    /* 8 bits, sin paridad, 1 stop */
    pUART->LCR = 0x03;

    /* Habilito transmisión */
    pUART->TER2 = 1;
}

/* ───────────────────────────────────────── */
uint8_t UARTDisponible(LPC_USART_T * pUART)
{
    return ((pUART->LSR & UART_LSR_THRE) != 0);
}

/* ───────────────────────────────────────── */
uint8_t UARTLeerByte(LPC_USART_T * pUART, uint8_t * data, uint8_t * error)
{
    uint8_t status = pUART->LSR;

    if (status & (1 << 0)) {      // RDR = hay dato
        if (status & 0x1E) {      // Cualquier error
            *error = status & 0x1E;
        } else {
            *data = pUART->RBR;
        }
        return 1;
    }

    return 0;
}

/* ───────────────────────────────────────── */
void UARTEscribirByte(LPC_USART_T * pUART, uint8_t data)
{
    while (!UARTDisponible(pUART)) {}
    pUART->THR = data;
}

void UARTEscribirString(LPC_USART_T * pUART, char string[])
{
    for (int i = 0; i < strlen(string); i++) {
        UARTEscribirByte(pUART, string[i]);
    }
}

/* ───────────────────────────────────────── */
/*        INTERRUPCIÓN UART2                 */
/* ───────────────────────────────────────── */
void UART2_IRQHandler(void)
{
    uint8_t status = USB_UART->LSR;

    /* Si hay error */
    if (status & 0x1E) {
        uart2_error = status & 0x1E;
        uart2_flag  = 1;
        uart2_byte  = USB_UART->RBR;  // igual hay que leerlo
        return;
    }

    /* Si hay dato sin error */
    if (status & (1 << 0)) { // RDR
        uart2_byte = USB_UART->RBR;
        uart2_error = 0;
        uart2_flag  = 1;
    }
}

/* ───────────────────────────────────────── */
/*       ACTIVAR INTERRUPCIÓN UART2          */
/* ───────────────────────────────────────── */
void ConfigurarInterrupcionUART2(void)
{
    /* Habilito interrupciones por RBR y por errores */
    USB_UART->IER = (1 << 0) | (1 << 2);

    NVIC_EnableIRQ(UART2_IRQn);
}

/* ───────────────────────────────────────── */
/*    DESACTIVAR INTERRUPCIÓN UART2          */
/* ───────────────────────────────────────── */
void DesactivarInterrupcionErrorUART2(void)
{
    /* Limpio el bit 2 del IER → desactiva interrupción por error */
    USB_UART->IER &= ~(1 << 2);
}


/* ───────────────────────────────────────── */
/*              MAIN                         */
/* ───────────────────────────────────────── */
int main(void)
{
    uint8_t readData = 0;
    uint8_t readError = 0;
    uint8_t buffer[20] = "";
    uint8_t* ptrBuffer = buffer;
    uint8_t contador = 0;

    ConfigurarPuertosLaboratorio();
    ConfigurarInterrupcion();
    ConfigurarUART(USB_UART);
    ConfigurarUART(MODEM_UART);
    ConfigurarInterrupcionUART2();   // <── Se agregó
    ConfigurarMODEM(USB_UART, MODEM_UART);

    while (1)
    {
        /* ─────────────────────────────── */
        /*         UART2 POR IRQ           */
        /* ─────────────────────────────── */
        if (uart2_flag) {
            uart2_flag = 0;

            if (uart2_error) {
                Led_Toggle(YELLOW_LED);

                if (uart2_error & (1 << 1))
                    strcpy(buffer, OVERRUN);
                else if (uart2_error & (1 << 2))
                    strcpy(buffer, PARITY);
                else if (uart2_error & (1 << 3))
                    strcpy(buffer, FRAMING);
                else if (uart2_error & (1 << 4))
                    strcpy(buffer, BREAK);
                else
                    strcpy(buffer, UNKNOWN);
            } else {
                // Eco del dato recibido en UART2
                UARTEscribirByte(USB_UART, uart2_byte);
            }
        }

        /* ─────────────────────────────── */
        /* UART3 (MODEM) Igual que el LAB */
        /* ─────────────────────────────── */
        if (UARTLeerByte(MODEM_UART, &readData, &readError)) {

            if (readError) {
                Led_Toggle(YELLOW_LED);

                if (readError & (1 << 1))
                    strcpy(buffer, OVERRUN);
                else if (readError & (1 << 2))
                    strcpy(buffer, PARITY);
                else if (readError & (1 << 3))
                    strcpy(buffer, FRAMING);
                else if (readError & (1 << 4))
                    strcpy(buffer, BREAK);
                else
                    strcpy(buffer, UNKNOWN);

                readError = 0;

            } else {
                // Tu lógica del contador
                Led_Toggle(RGB_B_LED);

                if (readData == '+') {
                    contador = (contador + 1) % 100;
                }
                if (readData == '-') {
                    contador = (contador == 0) ? 99 : contador - 1;
                }
                if (readData == '0') {
                    contador = 0;
                }

                buffer[0] = (contador / 10) + '0';
                buffer[1] = (contador % 10) + '0';
                buffer[2] = '\r';
                buffer[3] = '\n';
                buffer[4] = '\0';
            }
        }

        /* ─────────────────────────────── */
        /* Envío periódico (SysTick)   */
        /* ─────────────────────────────── */
        if (actualizar) {
            actualizar = 0;
            Led_On(GREEN_LED);

            while (*ptrBuffer != '\0') {
                UARTEscribirByte(USB_UART, *ptrBuffer);
                ptrBuffer++;
            }
            ptrBuffer = buffer;
            buffer[0] = '\0';

            Led_Off(GREEN_LED);
        }
    }
}

