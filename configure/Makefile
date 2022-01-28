# SPDX-FileCopyrightText: 2022 Cosylab d.d. https://www.cosylab.com
#
# SPDX-License-Identifier: CC0-1.0

TOP=..

include $(TOP)/configure/CONFIG

TARGETS = $(CONFIG_TARGETS)
CONFIGS += $(subst ../,,$(wildcard $(CONFIG_INSTALLS)))

include $(TOP)/configure/RULES
