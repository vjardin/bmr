################################################################################
# bmr
################################################################################
#BMR_VERSION = v1.0.0
BMR_VERSION = HEAD
BMR_SITE = $(call github,vjardin,bmr,$(BMR_VERSION))
BMR_LICENSE = AGPL
BMR_LICENSE_FILES = LICENSE COPYRIGHT
BMR_DEPENDENCIES = jansson i2c-tools

BMR_CONF_OPTS = -Dfully_static=$(if $(BR2_STATIC_LIBS),true,false)

$(eval $(meson-package))
