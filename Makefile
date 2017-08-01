include $(TOPDIR)/rules.mk

PKG_NAME:=ledtriggerd
PKG_VERSION:=0.1
PKG_RELEASE:=1
PKG_LICNESE:=GPL-2.0

include $(INCLUDE_DIR)/package.mk

define Package/ledtriggerd
  SECTION := TBD
  CATEGORY := TBD
  TITLE := Simple LED management daemon
  MAINTAINER := Thibaut VARENE <hacks@slashdirt.org>
endef

define Package/ledtriggerd/description
  A small process written in C to update a set of LEDs based on specific events
endef

define Build/Configure
endef

TARGET_LDFLAGS += -luci -lubox

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR) \
		CC="$(TARGET_CC)" \
		CFLAGS="$(TARGET_CFLAGS) -Wall" \
		LDFLAGS="$(TARGET_LDFLAGS)"
endef

define Package/ledtriggerd/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/ledtriggerd $(1)/usr/sbin/
endef

$(eval $(call BuildPackage,ledtriggerd))
