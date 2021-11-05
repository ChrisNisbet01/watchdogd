include $(TOPDIR)/rules.mk

PKG_NAME:=watchdogd
PKG_RELEASE:=1

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL=git@github.com:ChrisNisbet01/watchdogd.git
PKG_SOURCE_VERSION:=8bf720d0
CMAKE_INSTALL:=1

PKG_LICENSE:=LGPL-2.1
PKG_LICENSE_FILES:=
PKG_MAINTAINER:=Chris Nisbet <chris@nisbet.kiwi.nz>

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/watchdogd
  SECTION:=Niz
  CATEGORY:=My packages
  DEPENDS:=+libubus_utils +libubox
  TITLE:=Basic watchdog daemon
  MENU:=1
endef

TARGET_CFLAGS += -I$(STAGING_DIR)/usr/include -flto
TARGET_LDFLAGS += -flto

CMAKE_OPTIONS=
ifeq ($(CONFIG_WATCHDOGD_DEBUG),y)
CMAKE_OPTIONS += -DENABLE_DEBUG=ON
endif

define Package/watchdogd/config
	source "$(SOURCE)/Config.in"
endef

define Package/watchdogd/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_DIR) $(1)/etc/watchdog.d
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/bin/watchdogd $(1)/usr/bin/
endef

$(eval $(call BuildPackage,watchdogd))

