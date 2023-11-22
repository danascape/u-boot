// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2020
 * Sam Protsenko <joe.skb7@gmail.com>
 */

#include <android_image.h>
#include <command.h>
#include <image.h>
#include <malloc.h>
#include <mapmem.h>
#include <part.h>

#define abootimg_addr() \
	(_abootimg_addr == -1 ? image_load_addr : _abootimg_addr)

/* Please use abootimg_addr() macro to obtain the boot image address */
static ulong _abootimg_addr = -1;
static ulong _ainit_bootimg_addr = -1;
static ulong _avendor_bootimg_addr = -1;

ulong get_abootimg_addr(void)
{
	return (_abootimg_addr == -1 ? image_load_addr : _abootimg_addr);
}

void set_abootimg_addr(ulong addr)
{
	_abootimg_addr = addr;
}

ulong get_ainit_bootimg_addr(void)
{
	return _ainit_bootimg_addr;
}

ulong get_avendor_bootimg_addr(void)
{
	return _avendor_bootimg_addr;
}

void set_avendor_bootimg_addr(ulong addr)
{
	_avendor_bootimg_addr = addr;
}

static int abootimg_get_ver(int argc, char *const argv[])
{
	const struct andr_boot_img_hdr_v0 *hdr;
	int res = CMD_RET_SUCCESS;

	if (argc > 1)
		return CMD_RET_USAGE;

	hdr = map_sysmem(abootimg_addr(), sizeof(*hdr));
	if (!is_android_boot_image_header(hdr)) {
		printf("Error: Boot Image header is incorrect\n");
		res = CMD_RET_FAILURE;
		goto exit;
	}

	if (argc == 0)
		printf("%u\n", hdr->header_version);
	else
		env_set_ulong(argv[0], hdr->header_version);

exit:
	unmap_sysmem(hdr);
	return res;
}

static int abootimg_get_recovery_dtbo(int argc, char *const argv[])
{
	ulong addr;
	u32 size;

	if (argc > 2)
		return CMD_RET_USAGE;

	if (!android_image_get_dtbo(abootimg_addr(), &addr, &size))
		return CMD_RET_FAILURE;

	if (argc == 0) {
		printf("%lx\n", addr);
	} else {
		env_set_hex(argv[0], addr);
		if (argc == 2)
			env_set_hex(argv[1], size);
	}

	return CMD_RET_SUCCESS;
}

static int abootimg_get_dtb_load_addr(int argc, char *const argv[])
{
	if (argc > 1)
		return CMD_RET_USAGE;
	struct andr_image_data img_data = {0};
	const struct andr_boot_img_hdr_v0 *hdr;
	const struct andr_vnd_boot_img_hdr *vhdr;

	hdr = map_sysmem(abootimg_addr(), sizeof(*hdr));
	if (get_avendor_bootimg_addr() != -1)
		vhdr = map_sysmem(get_avendor_bootimg_addr(), sizeof(*vhdr));

	if (!android_image_get_data(hdr, vhdr, &img_data)) {
		if (get_avendor_bootimg_addr() != -1)
			unmap_sysmem(vhdr);
		unmap_sysmem(hdr);
		return CMD_RET_FAILURE;
	}

	if (get_avendor_bootimg_addr() != -1)
		unmap_sysmem(vhdr);
	unmap_sysmem(hdr);

	if (img_data.header_version < 2) {
		printf("Error: header_version must be >= 2 for this\n");
		return CMD_RET_FAILURE;
	}

	if (!img_data.dtb_load_addr) {
		printf("Error: failed to read dtb_load_addr\n");
		return CMD_RET_FAILURE;
	}

	if (argc == 0)
		printf("%lx\n", (ulong)img_data.dtb_load_addr);
	else
		env_set_hex(argv[0], (ulong)img_data.dtb_load_addr);

	return CMD_RET_SUCCESS;
}

static int abootimg_get_dtb_by_index(int argc, char *const argv[])
{
	const char *index_str;
	u32 num;
	char *endp;
	ulong addr;
	u32 size;

	if (argc < 1 || argc > 3)
		return CMD_RET_USAGE;

	index_str = argv[0] + strlen("--index=");
	if (index_str[0] == '\0') {
		printf("Error: Wrong index num\n");
		return CMD_RET_FAILURE;
	}

	num = simple_strtoul(index_str, &endp, 0);
	if (*endp != '\0') {
		printf("Error: Wrong index num\n");
		return CMD_RET_FAILURE;
	}

	if (!android_image_get_dtb_by_index(abootimg_addr(),
					    get_avendor_bootimg_addr(), num,
					    &addr, &size)) {
		return CMD_RET_FAILURE;
	}

	if (argc == 1) {
		printf("%lx\n", addr);
	} else {
		if (env_set_hex(argv[1], addr)) {
			printf("Error: Can't set [addr_var]\n");
			return CMD_RET_FAILURE;
		}

		if (argc == 3) {
			if (env_set_hex(argv[2], size)) {
				printf("Error: Can't set [size_var]\n");
				return CMD_RET_FAILURE;
			}
		}
	}

	return CMD_RET_SUCCESS;
}

static int abootimg_get_dtb(int argc, char *const argv[])
{
	if (argc < 1)
		return CMD_RET_USAGE;

	if (strstr(argv[0], "--index="))
		return abootimg_get_dtb_by_index(argc, argv);

	return CMD_RET_USAGE;
}

static int do_abootimg_addr(struct cmd_tbl *cmdtp, int flag, int argc,
			    char *const argv[])
{
	char *endp;
	ulong img_addr;

	if (argc < 2 || argc > 4)
		return CMD_RET_USAGE;

	img_addr = hextoul(argv[1], &endp);
	if (*endp != '\0') {
		printf("Error: Wrong image address\n");
		return CMD_RET_FAILURE;
	}

	_abootimg_addr = img_addr;

	if (argc > 2) {
		img_addr = simple_strtoul(argv[2], &endp, 16);
		if (*endp != '\0') {
			printf("Error: Wrong vendor_boot image address\n");
			return CMD_RET_FAILURE;
		}

		_avendor_bootimg_addr = img_addr;
	}

	if (argc == 4) {
		img_addr = simple_strtoul(argv[3], &endp, 16);
		if (*endp != '\0') {
			printf("Error: Wrong init_boot image address\n");
			return CMD_RET_FAILURE;
		}

		_ainit_bootimg_addr = img_addr;
	}

	return CMD_RET_SUCCESS;
}

static int do_abootimg_get(struct cmd_tbl *cmdtp, int flag, int argc,
			   char *const argv[])
{
	const char *param;

	if (argc < 2)
		return CMD_RET_USAGE;

	param = argv[1];
	argc -= 2;
	argv += 2;
	if (!strcmp(param, "ver"))
		return abootimg_get_ver(argc, argv);
	else if (!strcmp(param, "recovery_dtbo"))
		return abootimg_get_recovery_dtbo(argc, argv);
	else if (!strcmp(param, "dtb_load_addr"))
		return abootimg_get_dtb_load_addr(argc, argv);
	else if (!strcmp(param, "dtb"))
		return abootimg_get_dtb(argc, argv);

	return CMD_RET_USAGE;
}

static int do_abootimg_dump(struct cmd_tbl *cmdtp, int flag, int argc,
			    char *const argv[])
{
	if (argc != 2)
		return CMD_RET_USAGE;

	if (!strcmp(argv[1], "dtb")) {
		if (android_image_print_dtb_contents(abootimg_addr()))
			return CMD_RET_FAILURE;
	} else {
		return CMD_RET_USAGE;
	}

	return CMD_RET_SUCCESS;
}

static int do_abootimg_load(struct cmd_tbl *cmdtp, int flag, int argc,
			    char *const argv[])
{
	int time_start = get_timer(0);
	struct blk_desc *desc;
	struct disk_partition info;
	char buf[512] = { 0 };
	void *addr;
	int ret;

	if (argc < 4)
		return CMD_RET_USAGE;
	if (argc > 5)
		return CMD_RET_USAGE;

	ret = blk_get_device_by_str(argv[1], argv[2], &desc);
	if (ret < 0) {
		printf("Error: Failed to get device %s %s\n", argv[1], argv[2]);
		return CMD_RET_FAILURE;
	}

	if (argc == 5)
		sprintf(buf, "%s_%s", argv[3], argv[4]);
	else
		sprintf(buf, "%s", argv[3]);

	ret = part_get_info_by_name(desc, buf, &info);
	if (ret < 0) {
		printf("Error: Failed to get partition %s\n", buf);
		return CMD_RET_FAILURE;
	}

	addr = (void *)memalign(4096, info.size * info.blksz);
	if (!addr) {
		printf("Error: Failed to allocate memory\n");
		return CMD_RET_FAILURE;
	}

	ret = blk_dread(desc, info.start, info.size, addr);
	if (ret < 0) {
		printf("Error: Failed to read partition %s\n", buf);
		goto fail;
	}

	sprintf(buf, "abootimg_%s_ptr", argv[3]);
	env_set_hex(buf, (ulong)addr);

	sprintf(buf, "abootimg_%s_size", argv[3]);
	env_set_hex(buf, info.size * info.blksz);

	if (argc == 5) {
		sprintf(buf, "abootimg_%s_%s_ptr", argv[3], argv[4]);
		env_set_hex(buf, (ulong)addr);

		sprintf(buf, "abootimg_%s_%s_size", argv[3], argv[4]);
		env_set_hex(buf, info.size * info.blksz);
	}

	int time_end = get_timer(0);

	printf("Loaded '%s' partition to address 0x%p (size: 0x%x) in %lu ms\n",
	       argv[3], addr, info.size * info.blksz, time_end - time_start);

	return CMD_RET_SUCCESS;

fail:
	free(addr);
	return CMD_RET_FAILURE;
}

static struct cmd_tbl cmd_abootimg_sub[] = {
	U_BOOT_CMD_MKENT(addr, 4, 1, do_abootimg_addr, "", ""),
	U_BOOT_CMD_MKENT(dump, 2, 1, do_abootimg_dump, "", ""),
	U_BOOT_CMD_MKENT(get, 5, 1, do_abootimg_get, "", ""),
	U_BOOT_CMD_MKENT(load, 5, 1, do_abootimg_load, "", ""),
};

static int do_abootimg(struct cmd_tbl *cmdtp, int flag, int argc,
		       char *const argv[])
{
	struct cmd_tbl *cp;

	cp = find_cmd_tbl(argv[1], cmd_abootimg_sub,
			  ARRAY_SIZE(cmd_abootimg_sub));

	/* Strip off leading 'abootimg' command argument */
	argc--;
	argv++;

	if (!cp || argc > cp->maxargs)
		return CMD_RET_USAGE;
	if (flag == CMD_FLAG_REPEAT && !cmd_is_repeatable(cp))
		return CMD_RET_SUCCESS;

	return cp->cmd(cmdtp, flag, argc, argv);
}

U_BOOT_CMD(
	abootimg, CONFIG_SYS_MAXARGS, 0, do_abootimg,
	"manipulate Android Boot Image",
	"addr <boot_img_addr> [<vendor_boot_img_addr> [<init_boot_img_addr>]]\n"
	"    - set the address in RAM where boot image is located\n"
	"      ($loadaddr is used by default)\n"
	"abootimg dump dtb\n"
	"    - print info for all DT blobs in DTB area\n"
	"abootimg get ver [varname]\n"
	"    - get header version\n"
	"abootimg get recovery_dtbo [addr_var [size_var]]\n"
	"    - get address and size (hex) of recovery DTBO area in the image\n"
	"      [addr_var]: variable name to contain DTBO area address\n"
	"      [size_var]: variable name to contain DTBO area size\n"
	"abootimg get dtb_load_addr [varname]\n"
	"    - get load address (hex) of DTB, from image header\n"
	"abootimg get dtb --index=<num> [addr_var [size_var]]\n"
	"    - get address and size (hex) of DT blob in the image by index\n"
	"      <num>: index number of desired DT blob in DTB area\n"
	"      [addr_var]: variable name to contain DT blob address\n"
	"      [size_var]: variable name to contain DT blob size\n"
	"abootimg load interface dev partition [slot_name]\n"
	"    - load boot image from device partition\n"
	"      memory is allocated in heap\n"
	"      address is stored in $abootimg_<partition>_ptr\n"
	"      size is stored in $abootimg_<partition>_size\n"
	"      interface: interface type (e.g. mmc, usb)\n"
	"      dev: device number (e.g. 0, 1)\n"
	"      partition: partition number (e.g. boot, dtb)\n"
	"      slot_suffix: slot name (e.g. a, b)"
);
