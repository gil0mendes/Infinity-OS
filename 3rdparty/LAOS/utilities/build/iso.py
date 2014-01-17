#
# Copyright (c) 2012 DesingTure. All rights reserved.
# 
# This file contains the original code and/or modify of original code as
# defined in.
# All code in this file is property of DesignTure project, this code should
# not be distributed or commercialized.
# Only those involved in this project will have access to the code and no one
# else can see.
#

from SCons.Script import *

# Function to generate an ISO image
def iso_image_func(target, source, env):
        import os, sys, tempfile, shutil
        config = env['CONFIG']

        cdboot = str(env['CDBOOT'])
	loader  = str(env['LOADER'])

        # Create the work directory
        tmpdir = tempfile.mkdtemp('.laosiso')
        os.makedirs(os.path.join(tmpdir, 'boot'))

        # Create the loader by contenating the CD boot sector and the loader
        # together
        f = open(os.path.join(tmpdir, 'boot', 'cdboot.img'), 'w')
        f.write(open(cdboot, 'r').read())
        f.close()

        # Create the ISO
	verbose = (ARGUMENTS.get('V') == '1') and '' or '>> /dev/null 2>&1'
	if os.system('mkisofs -J -R -l -b boot/cdboot.img -V "LAOS CDROM" ' + \
	             '-boot-load-size 4 -boot-info-table -no-emul-boot ' + \
	             '-o %s %s %s' % (target[0], tmpdir, verbose)) != 0:
		print "Could not find mkisofs! Please ensure that it is installed."
		shutil.rmtree(tmpdir)
		return 1
    
	# Clean up.
	#shutil.rmtree(tmpdir)
	return 0

def iso_image_emitter(target, source, env): 
	return (target, source + [env['CDBOOT']])

ISOBuilder = Builder(action = Action(iso_image_func, '$GENCOMSTR'), emitter = iso_image_emitter)