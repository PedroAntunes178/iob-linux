/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Western Digital Corporation or its affiliates.
 */

#include <sbi/riscv_asm.h>
#include <sbi/riscv_encoding.h>
#include <sbi/riscv_io.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_const.h>
#include <sbi/sbi_hart.h>
#include <sbi/sbi_platform.h>

/*
 * Include these files as needed.
 * See config.mk PLATFORM_xxx configuration parameters.
 */
#include <sbi_utils/fdt/fdt_fixup.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi_utils/ipi/aclint_mswi.h>
#include <sbi_utils/irqchip/plic.h>
#include <sbi_utils/serial/uart8250.h>
#include <sbi_utils/timer/aclint_mtimer.h>

#define IOB_SOC_PLIC_ADDR 0xFC000000
#define IOB_SOC_PLIC_NUM_SOURCES 32
#define IOB_SOC_HART_COUNT 1
#define IOB_SOC_CLINT_ADDR 0xF8000000
#define IOB_SOC_ACLINT_MTIMER_FREQ 100000000
#define IOB_SOC_ACLINT_MSWI_ADDR (IOB_SOC_CLINT_ADDR + CLINT_MSWI_OFFSET)
#define IOB_SOC_ACLINT_MTIMER_ADDR (IOB_SOC_CLINT_ADDR + CLINT_MTIMER_OFFSET)
#define IOB_SOC_UART_ADDR 0xF4000000
#define IOB_SOC_UART_INPUT_FREQ 100000000
#define IOB_SOC_UART_BAUDRATE 115200

static struct platform_uart_data uart = {
    IOB_SOC_UART_ADDR,
    IOB_SOC_UART_INPUT_FREQ,
    IOB_SOC_UART_BAUDRATE,
};

static struct plic_data plic = {
    .addr = IOB_SOC_PLIC_ADDR,
    .num_src = IOB_SOC_PLIC_NUM_SOURCES,
};

static struct aclint_mswi_data mswi = {
    .addr = IOB_SOC_ACLINT_MSWI_ADDR,
    .size = ACLINT_MSWI_SIZE,
    .first_hartid = 0,
    .hart_count = IOB_SOC_HART_COUNT,
};

static struct aclint_mtimer_data mtimer = {
    .mtime_freq = IOB_SOC_ACLINT_MTIMER_FREQ,
    .mtime_addr = IOB_SOC_ACLINT_MTIMER_ADDR + ACLINT_DEFAULT_MTIME_OFFSET,
    .mtime_size = ACLINT_DEFAULT_MTIME_SIZE,
    .mtimecmp_addr =
        IOB_SOC_ACLINT_MTIMER_ADDR + ACLINT_DEFAULT_MTIMECMP_OFFSET,
    .mtimecmp_size = ACLINT_DEFAULT_MTIMECMP_SIZE,
    .first_hartid = 0,
    .hart_count = IOB_SOC_HART_COUNT,
    .has_64bit_mmio = TRUE,
};

/*
 * Platform early initialization.
 */
static int iob_soc_early_init(bool cold_boot) {
  void *fdt;
  struct platform_uart_data uart_data;
  struct plic_data plic_data;
  unsigned long aclint_freq;
  uint64_t clint_addr;
  int rc;

  if (!cold_boot)
    return 0;
  fdt = fdt_get_address();

  rc = fdt_parse_uart8250(fdt, &uart_data, "ns16550");
  if (!rc)
    uart = uart_data;

  rc = fdt_parse_plic(fdt, &plic_data, "riscv,plic0");
  if (!rc)
    plic = plic_data;

  rc = fdt_parse_timebase_frequency(fdt, &aclint_freq);
  if (!rc)
    mtimer.mtime_freq = aclint_freq;

  rc = fdt_parse_compat_addr(fdt, &clint_addr, "riscv,clint0");
  if (!rc) {
    mswi.addr = clint_addr;
    mtimer.mtime_addr =
        clint_addr + CLINT_MTIMER_OFFSET + ACLINT_DEFAULT_MTIME_OFFSET;
    mtimer.mtimecmp_addr =
        clint_addr + CLINT_MTIMER_OFFSET + ACLINT_DEFAULT_MTIMECMP_OFFSET;
  }

  return 0;
}

/*
 * Platform final initialization.
 */
static int iob_soc_final_init(bool cold_boot) {
  void *fdt;

  if (!cold_boot)
    return 0;

  fdt = fdt_get_address();
  fdt_fixups(fdt);

  return 0;
}

/*
 * Initialize the iob_soc console.
 */
static int iob_soc_console_init(void) {
  /* Example if the generic UART8250 driver is used */
  return uart8250_init(IOB_SOC_UART_ADDR, IOB_SOC_UART_INPUT_FREQ,
                       IOB_SOC_UART_BAUDRATE, 0, 1, 0);
}

/*
 * Initialize the iob_soc interrupt controller for current HART.
 */
static int iob_soc_irqchip_init(bool cold_boot) {
  u32 hartid = current_hartid();
  int ret;

  /* Example if the generic PLIC driver is used */
  if (cold_boot) {
    ret = plic_cold_irqchip_init(&plic);
    if (ret)
      return ret;
  }

  return plic_warm_irqchip_init(&plic, 2 * hartid, 2 * hartid + 1);
}

/*
 * Initialize IPI for current HART.
 */
static int iob_soc_ipi_init(bool cold_boot) {
  int ret;

  if (cold_boot) {
    ret = aclint_mswi_cold_init(&mswi);
    if (ret)
      return ret;
  }

  return aclint_mswi_warm_init();
}

/*
 * Initialize iob_soc timer for current HART.
 */
static int iob_soc_timer_init(bool cold_boot) {
  int ret;

  if (cold_boot) {
    ret = aclint_mtimer_cold_init(&mtimer, NULL);
    if (ret)
      return ret;
  }

  return aclint_mtimer_warm_init();
}

/*
 * Platform descriptor.
 */
const struct sbi_platform_operations platform_ops = {
    .early_init = iob_soc_early_init,
    .final_init = iob_soc_final_init,
    .console_init = iob_soc_console_init,
    .irqchip_init = iob_soc_irqchip_init,
    .ipi_init = iob_soc_ipi_init,
    .timer_init = iob_soc_timer_init};
const struct sbi_platform platform = {
    .opensbi_version = OPENSBI_VERSION,
    .platform_version = SBI_PLATFORM_VERSION(0x0, 0x01),
    .name = "iob-soc",
    .features = SBI_PLATFORM_DEFAULT_FEATURES,
    .hart_count = IOB_SOC_HART_COUNT,
    .hart_stack_size = SBI_PLATFORM_DEFAULT_HART_STACK_SIZE,
    .platform_ops_addr = (unsigned long)&platform_ops};
