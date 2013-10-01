#!/usr/bin/env python
import os
import shutil
from waflib import Logs
from waflib import Options
from waflib.Configure import conf

# Variables for 'waf dist'
APPNAME = 'lmms.lv2'
VERSION = '0.1.0'

LV2DIR  = '/usr/local/lib/lv2'

# Mandatory variables
top = '.'
out = 'build'

@conf
def check_sizeof(ctx, t):
    frag = '#include <cstdio> \n int main() { printf("%%zu", sizeof(%s)); }' 
    ctx.start_msg('Checking size of %s' % t)
    sz = ctx.check_cxx(fragment=frag % t, execute=True,
                       define_ret=True, quote=False)
    ctx.end_msg(sz)
    return int(sz)


def options(opt):
    opt.load('compiler_c compiler_cxx')

    opt.add_option('--max-polyphony', dest='max_polyphony', type='int', default=8,
                   help='Maximum instrument polyphony (static parameter for now)')


def configure(conf):
    conf.load('compiler_c compiler_cxx')
    conf.env.append_unique('CFLAGS',
        ['-Wall', '-ggdb', '-std=c99', '-O3', '-ffast-math', '-fPIC'])
    conf.env.append_unique('CXXFLAGS',
        ['-Wall', '-ggdb', '-O3', '-ffast-math', '-fPIC'])

    # TODO: don't hardcode
    conf.env.LV2DIR = LV2DIR;

    # Required
    conf.check_cc(lib='m', uselib_store='M',
            msg="Checking for 'libm' (math library)")

    conf.check_cfg(package='lv2core', atleast_version='6.0',
            args=['--cflags', '--libs'])

    conf.check_cfg(package='lv2', atleast_version='1.0.0',
            uselib_store='LV2', args=['--cflags', '--libs'])

    # Optional
    conf.check_cfg(package='gtk+-2.0', atleast_version='2.18.0',
            uselib_store='GTK2', args=['--cflags', '--libs'], 
            mandatory=False)

    conf.find_program('perl', var='PERL')

    # Check language features (Need to expand this)
    conf.check_cxx(fragment='int main() { bool flag; }',
                   define_name='HAVE_BOOL',
                   msg='Checking for working bool')

    sizeof_int = conf.check_sizeof('int')
    if sizeof_int < 4:
        conf.fatal('only 32 bit or better CPUs are supported')

    # Pre-calculate pattern for plugin *.so files
    pat = conf.env['cshlib_PATTERN']
    if pat.startswith('lib'):
        pat = pat[3:]
    conf.env['pluginlib_PATTERN'] = pat

    # Additional macro definitions
    conf.define('NUM_VOICES', Options.options.max_polyphony)

    conf.write_config_header('src/config.h')

    conf.recurse('3rdparty src')


def build(bld):
    # TODO Remove this redundant installdir calculation
    bundle     = 'lmms.lv2'
    installdir = os.path.join(bld.env.LV2DIR, bundle)

    bld.recurse('3rdparty src tests')

    bld.install_files(installdir, bld.path.ant_glob('presets/**/*.ttl'),
                      relative_trick=True)

# vim: ts=8:sts=4:sw=4:et
