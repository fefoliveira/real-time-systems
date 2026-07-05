#ifndef ESP32C3_REGS_H
#define ESP32C3_REGS_H

// =============================================================================
// SYSTIMER REGISTERS — offsets from TRM v1.4 Chapter 10 register summary
// =============================================================================
#define SYSTIMER_BASE           0x60023000UL

#define SYSTIMER_CONF           (*(volatile uint32_t *)(SYSTIMER_BASE + 0x0000))
#define SYSTIMER_UNIT0_OP       (*(volatile uint32_t *)(SYSTIMER_BASE + 0x0004))

// UNIT0 load (write new counter value)
#define SYSTIMER_UNIT0_LOAD_HI  (*(volatile uint32_t *)(SYSTIMER_BASE + 0x000C))
#define SYSTIMER_UNIT0_LOAD_LO  (*(volatile uint32_t *)(SYSTIMER_BASE + 0x0010))
#define SYSTIMER_UNIT0_LOAD     (*(volatile uint32_t *)(SYSTIMER_BASE + 0x005C))

// UNIT0 snapshot read (read-only, populated after UPDATE)
#define SYSTIMER_UNIT0_VALUE_HI (*(volatile uint32_t *)(SYSTIMER_BASE + 0x0040))
#define SYSTIMER_UNIT0_VALUE_LO (*(volatile uint32_t *)(SYSTIMER_BASE + 0x0044))

// COMP0 (TARGET0) — alarm config
#define SYSTIMER_TARGET0_HI     (*(volatile uint32_t *)(SYSTIMER_BASE + 0x001C))
#define SYSTIMER_TARGET0_LO     (*(volatile uint32_t *)(SYSTIMER_BASE + 0x0020))
#define SYSTIMER_TARGET0_CONF   (*(volatile uint32_t *)(SYSTIMER_BASE + 0x0034))
#define SYSTIMER_COMP0_LOAD     (*(volatile uint32_t *)(SYSTIMER_BASE + 0x0050))

// Interrupt registers
#define SYSTIMER_INT_ENA        (*(volatile uint32_t *)(SYSTIMER_BASE + 0x0064))
#define SYSTIMER_INT_RAW        (*(volatile uint32_t *)(SYSTIMER_BASE + 0x0068))
#define SYSTIMER_INT_CLR        (*(volatile uint32_t *)(SYSTIMER_BASE + 0x006C))
#define SYSTIMER_INT_ST         (*(volatile uint32_t *)(SYSTIMER_BASE + 0x0070))


// =============================================================================
// INTERRUPT MATRIX REGISTERS — offsets from TRM v1.4 Chapter 8
// =============================================================================
#define INTMTX_BASE             0x600C2000UL

#define INTMTX_GPIO_MAP         (*(volatile uint32_t *)(INTMTX_BASE + 0x0040))
#define INTMTX_SYSTIMER_T0_MAP  (*(volatile uint32_t *)(INTMTX_BASE + 0x0094))
#define INTCTL_ENABLE           (*(volatile uint32_t *)(INTMTX_BASE + 0x0104))
#define INTCTL_TYPE             (*(volatile uint32_t *)(INTMTX_BASE + 0x0108))
#define INTCTL_CLEAR            (*(volatile uint32_t *)(INTMTX_BASE + 0x010C))
#define INTCTL_PRI(n)           (*(volatile uint32_t *)(INTMTX_BASE + 0x0114 + (n)*4))
#define INTCTL_THRESH           (*(volatile uint32_t *)(INTMTX_BASE + 0x0194))

// =============================================================================
// GPIO REGISTERS
// =============================================================================
#define GPIO_STATUS             (*(volatile uint32_t *)(C3_GPIO + 0x044))
#define GPIO_STATUS_W1TC        (*(volatile uint32_t *)(C3_GPIO + 0x04C))
#define GPIO_PIN(n)             (*(volatile uint32_t *)(C3_GPIO + 0x074 + (n)*4))

// =============================================================================
// UART-USB REGISTERS
// =============================================================================
#define UART_USB_BASE           0x60043000UL
#define UART_USB_AVAIL          (*(volatile uint32_t *)(UART_USB_BASE + 0x004))
#define UART_USB_EP1_DATA       (*(volatile uint32_t *)(UART_USB_BASE + 0x000))


// =============================================================================
// PIN / INTERRUPT ASSIGNMENTS
// =============================================================================
#define BUILT_IN_LED     8


#define serial_available_usb() (UART_USB_AVAIL & 0x4)
#define serial_read_usb() (UART_USB_EP1_DATA)
#endif // ESP32C3_REGS_H