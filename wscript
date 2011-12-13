#!/usr/bin/env python
import os
import shutil
from waflib import Logs

# Variables for 'waf dist'
APPNAME = 'lb303.lv2'
VERSION = '0.1.0'

# Mandatory variables
top = '.'
out = 'build'

def options(opt):
    opt.load('compiler_c')

def configure(conf):
    conf.load('compiler_c')

    conf.check_cfg(package='lv2core', atleast_version='6.0',
            args=['--cflags', '--libs'])
    conf.check_cfg(package='lv2-lv2plug.in-ns-ext-urid',
            uselib_store='LV2_URID', args=['--cflags', '--libs'])
    conf.check_cfg(package='lv2-lv2plug.in-ns-ext-atom',
            uselib_store='LV2_ATOM', args=['--cflags', '--libs'])
    conf.check_cfg(package='lv2-lv2plug.in-ns-ext-state',
            uselib_store='LV2_STATE', args=['--cflags', '--libs'])
    conf.check_cfg(package='gtk+-2.0', atleast_version='2.18.0',
            uselib_store='GTK2', args=['--cflags', '--libs'], 
            mandatory=False)

def build(bld):
    pass

# vim: ts=8:sts=4:sw=4:et
