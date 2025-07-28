include $(TOPDIR)/rules.mk

PKG_NAME:=device-discovery
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

define Package/device-discovery
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=Device Discovery
  DEPENDS:=+libstdcpp +libsqlite3 +libpcap +libjson-c
  MAINTAINER:=Marek Puzyniak <marek.puzyniak@holisticon.pl>
endef

define Package/device-discovery/description
  A C++ application to be run on prplOS.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR) \
		CC="$(TARGET_CC)" \
		CXX="$(TARGET_CXX)" \
		CFLAGS="$(TARGET_CFLAGS)" \
		CXXFLAGS="$(TARGET_CXXFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS)"
endef

define Package/device-discovery/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/device-discovery $(1)/usr/bin/
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_CONF) ./files/device_discovery.conf $(1)/etc/config/device_discovery
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/device_discovery.init $(1)/etc/init.d/device_discovery
endef

$(eval $(call BuildPackage,device-discovery))

