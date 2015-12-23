s/^#ifndef\ _H8300/#ifndef __ASM_RX/g
s/^#define\ _H8300/#define __ASM_RX/g
s/^#ifndef\ __ARCH_H8300/#ifndef\ __ASM_RX/g
s/^#define\ __ARCH_H8300/#define\ __ASM_RX/g
s/^#ifndef\ _ASM_H8300/#ifndef __ASM_RX/g
s/^#define\ _ASM_H8300/#define __ASM_RX/g
s/^#ifndef\ __ASMH8300/#ifndef __ASM_RX/g
s/^#define\ __ASMH8300/#define __ASM_RX/g
s/^#ifndef\ __H8300/#ifndef __ASM_RX/g
s/^#define\ __H8300/#define __ASM_RX/g
s/^#\(.*\)H_*$/#\1H__/g
