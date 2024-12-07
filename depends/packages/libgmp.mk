package=libgmp
$(package)_version=6.3.0
$(package)_download_path=https://gmplib.org/download/gmp
$(package)_file_name=gmp-$($(package)_version).tar.xz
$(package)_sha256_hash=a3c2b80201b89e68616f4ad30bc66aee4927c3ce50e33929ca819d5c43538898

define $(package)_set_vars
$(package)_config_opts=--enable-cxx --disable-assembly --disable-shared --enable-static --with-pic
endef

define $(package)_config_cmds
  $($(package)_autoconf)
endef

define $(package)_build_cmds
  $(MAKE) -j$(JOBS)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef
