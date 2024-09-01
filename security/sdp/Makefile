#
# Makefile for DEK and DLP modules
#

obj-$(CONFIG_SDP) += dek_aes.o sdp_mm.o pub_crypto_emul.o dek_sysfs.o cache_cleanup.o kek_pack.o fs_handler.o
ifdef CONFIG_FSCRYPT_SDP
obj-$(CONFIG_SDP) += dek.o
else
obj-$(CONFIG_SDP) += dek_legacy.o 
endif
obj-$(CONFIG_DLP) += sdp_dlp.o
obj-$(CONFIG_DDAR) += dd_core.o dd_xattr.o dd_kernel_crypto.o
