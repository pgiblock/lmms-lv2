#!/usr/bin/env python
import os
import shutil
from waflib import Logs
from waflib import Options

# Variables for 'waf dist'
APPNAME = 'lmms.lv2'
VERSION = '0.1.0'

LV2DIR  = '/usr/local/lib/lv2'

# Mandatory variables
top = '.'
out = 'build'

def options(opt):
    opt.load('compiler_c')

    opt.add_option('--max-polyphony', dest='max_polyphony', type='int', default=8,
                   help='Maximum instrument polyphony (static parameter for now)')

def configure(conf):
    conf.load('compiler_c')
    conf.env.append_value('CFLAGS', ['-Wall', '-ggdb', '-std=c99', '-fPIC'])

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

    # Pre-calculate pattern for plugin *.so files
    pat = conf.env['cshlib_PATTERN']
    if pat.startswith('lib'):
        pat = pat[3:]
    conf.env['pluginlib_PATTERN'] = pat

    # Additional macro definitions
    conf.define('NUM_VOICES', Options.options.max_polyphony)

    conf.write_config_header('src/config.h')


def build(bld):
    bld.recurse('src')

# vim: ts=8:sts=4:sw=4:et
