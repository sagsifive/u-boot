// SPDX-License-Identifier: GPL-2.0+
/*
 * SiFive GPIO driver
 *
 * Copyright (C) 2019 SiFive, Inc.
 */

#include <common.h>
#include <dm.h>
#include <asm/arch/gpio.h>
#include <asm/io.h>
#include <errno.h>
#include <asm/gpio.h>

static int fu540_gpio_probe(struct udevice *dev)
{
	struct gpio_dev_priv *uc_priv = dev_get_uclass_priv(dev);

	uc_priv->bank_name = dev->name;

	/*
	 * Use the gpio count mentioned in device tree,
	 * if not specified in dt, set NR_GPIOS as default
	 */
	uc_priv->gpio_count = dev_read_u32_default(dev, "ngpios", NR_GPIOS);

	return 0;
}

static void fu540_update_gpio_reg(void *bptr, u32 offset, bool value)
{
	void __iomem *ptr = (void __iomem *)bptr;

	u32 bit = BIT(offset);
	u32 old = readl(ptr);

	if (value)
		writel(old | bit, ptr);
	else
		writel(old & ~bit, ptr);
}

static int fu540_gpio_direction_input(struct udevice *dev, u32 offset)
{
	struct fu540_gpio_platdata *plat = dev_get_platdata(dev);
	struct gpio_dev_priv *uc_priv = dev_get_uclass_priv(dev);

	if (offset > uc_priv->gpio_count)
		return -EINVAL;

	/* Configure GPIO direction as input */
	fu540_update_gpio_reg(plat->base + GPIO_INPUT_EN,  offset, true);
	fu540_update_gpio_reg(plat->base + GPIO_OUTPUT_EN, offset, false);

	return 0;
}

static int fu540_gpio_direction_output(struct udevice *dev, u32 offset,
				       int value)
{
	struct fu540_gpio_platdata *plat = dev_get_platdata(dev);
	struct gpio_dev_priv *uc_priv = dev_get_uclass_priv(dev);

	if (offset > uc_priv->gpio_count)
		return -EINVAL;

	/* Configure GPIO direction as output */
	fu540_update_gpio_reg(plat->base + GPIO_OUTPUT_EN, offset, true);
	fu540_update_gpio_reg(plat->base + GPIO_INPUT_EN,  offset, false);

	/* Set the Output state of the PIN */
	fu540_update_gpio_reg(plat->base + GPIO_OUTPUT_VAL, offset, value);

	return 0;
}

static int fu540_gpio_get_value(struct udevice *dev, u32 offset)
{
	struct fu540_gpio_platdata *plat = dev_get_platdata(dev);
	struct gpio_dev_priv *uc_priv = dev_get_uclass_priv(dev);
	int val;
	int dir;

	if (offset > uc_priv->gpio_count)
		return -EINVAL;

	/* Get direction of the pin OUTPUT=0 INPUT=1 */
	dir = !(readl(plat->base + GPIO_OUTPUT_EN) & BIT(offset));

	if (dir)
		val = readl(plat->base + GPIO_INPUT_VAL) & BIT(offset);
	else
		val = readl(plat->base + GPIO_OUTPUT_VAL) & BIT(offset);

	return val ? HIGH : LOW;
}

static int fu540_gpio_set_value(struct udevice *dev, u32 offset, int value)
{
	struct fu540_gpio_platdata *plat = dev_get_platdata(dev);

	if (offset > NR_GPIOS)
		return -EINVAL;

	fu540_update_gpio_reg(plat->base + GPIO_OUTPUT_VAL, offset, value);

	return 0;
}

static const struct udevice_id fu540_gpio_match[] = {
	{ .compatible = "sifive,gpio0" },
	{ }
};

static const struct dm_gpio_ops gpio_sifive_ops = {
	.direction_input        = fu540_gpio_direction_input,
	.direction_output       = fu540_gpio_direction_output,
	.get_value              = fu540_gpio_get_value,
	.set_value              = fu540_gpio_set_value,
};

static int fu540_gpio_ofdata_to_platdata(struct udevice *dev)
{
	struct fu540_gpio_platdata *plat = dev_get_platdata(dev);
	fdt_addr_t addr;

	addr = devfdt_get_addr(dev);
	if (addr == FDT_ADDR_T_NONE)
		return -EINVAL;

	plat->base = (void *)addr;
	return 0;
}

U_BOOT_DRIVER(gpio_sifive) = {
	.name	= "gpio_sifive",
	.id	= UCLASS_GPIO,
	.of_match = fu540_gpio_match,
	.ofdata_to_platdata = of_match_ptr(fu540_gpio_ofdata_to_platdata),
	.platdata_auto_alloc_size = sizeof(struct fu540_gpio_platdata),
	.ops	= &gpio_sifive_ops,
	.probe	= fu540_gpio_probe,
};
