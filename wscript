#!/usr/bin/env python
import os
import shutil
from waflib import Logs

# Variables for 'waf dist'
APPNAME = 'lmms.lv2'
VERSION = '0.1.0'

LV2DIR  = '/usr/local/lib/lv2'

# Mandatory variables
top = '.'
out = 'build'

def options(opt):
    opt.load('compiler_c')

    opt.add_option('--use-urid', dest='use_urid', default=False, action='store_true',
                   help='Use the urid extension in instead of uri-map')
    opt.add_option('--use-atom', dest='use_atom', default=False, action='store_true',
                   help='Use the atom extension in instead of event')

def configure(conf):
    conf.load('compiler_c')
    conf.env.append_value('CFLAGS', '-Wall')
    conf.env.append_value('CFLAGS', '-ggdb')
    conf.env.append_value('CFLAGS', '-std=c99')
    conf.env.append_value('CFLAGS', '-fPIC')

    # TODO: don't hardcode
    conf.env.LV2DIR = LV2DIR;

    # Required
    conf.check_cfg(package='lv2core', atleast_version='6.0',
            args=['--cflags', '--libs'])

    conf.check_cfg(package='lv2', atleast_version='1.0.0',
            uselib_store='LV2', args=['--cflags', '--libs'])

    # Optional
    conf.check_cfg(package='gtk+-2.0', atleast_version='2.18.0',
            uselib_store='GTK2', args=['--cflags', '--libs'], 
            mandatory=False)

    conf.define('USE_LV2_URID', 1)
    conf.define('USE_LV2_ATOM', 1)

    # Pre-calculate pattern for plugin *.so files
    pat = conf.env['cshlib_PATTERN']
    if pat.startswith('lib'):
        pat = pat[3:]
    conf.env['pluginlib_PATTERN'] = pat

    conf.write_config_header('src/config.h')


def build(bld):
    bld.recurse('src')

# vim: ts=8:sts=4:sw=4:et
