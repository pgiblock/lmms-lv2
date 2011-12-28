#!/usr/bin/env python
import os
import shutil
from waflib import Logs

# Variables for 'waf dist'
APPNAME = 'lb303.lv2'
VERSION = '0.1.0'

LV2DIR  = '/usr/local/lib/lv2'

# Mandatory variables
top = '.'
out = 'build'

def options(opt):
    opt.load('compiler_c')

def configure(conf):
    conf.load('compiler_c')
    conf.env.append_value('CFLAGS', '-std=c99')

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

    pat = conf.env['cshlib_PATTERN']
    if pat.startswith('lib'):
        pat = pat[3:]
    conf.env['pluginlib_PATTERN'] = pat

def build(bld):
    bundle     = 'lb303.lv2'
    installdir = os.path.join(LV2DIR, bundle)

    # Plugin environment
    penv                   = bld.env.derive()
    penv['cshlib_PATTERN'] = bld.env['pluginlib_PATTERN']

    bld.shlib(source='lb303.c', target='%s/lb303' % bundle, install_path=installdir, env=penv)
    for f in ['manifest.ttl', 'lb303.ttl']:
        bld(features='subst', source=f, target=os.path.join(bundle,f), install_path=installdir)

# vim: ts=8:sts=4:sw=4:et
