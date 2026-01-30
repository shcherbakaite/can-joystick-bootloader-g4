/*
 * bootloader.h
 *
 *  Created on: Jan 28, 2026
 *      Author: vlad.shcherbakov
 */

#ifndef INC_BOOTLOADER_H_
#define INC_BOOTLOADER_H_

void bootloader_init();

void bootloader_read_version();

void bootloader_read_crc();

void bootloader_get_application_crc();

void bootloader_erase_application();

#endif /* INC_BOOTLOADER_H_ */
