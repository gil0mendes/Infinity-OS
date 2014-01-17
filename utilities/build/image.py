#
# Copyright (C) 2012-2013 Designture
#
# Permission is hereby granted, free of charge, to any person obtaining a copy of
# this software and associated documentation files (the "Software"), to deal in
# the Software without restriction, including without limitation the rights to
# use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
# the Software, and to permit persons to whom the Software is furnished to do so,
# subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
# FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
# COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
# IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

from SCons.Script import *
import tarfile, glob, os, tempfile, shutil, time

# Create a TAR archive containing the filesystem tree.
def fs_image_func(target, source, env):
	config = env['_CONFIG']

	# Create the TAR file.
	tar = tarfile.open(str(target[0]), 'w')

	def make_dir(name):
		if len(name) == 0:
			return
		try:
			tar.getmember(name)
		except KeyError:
			make_dir(os.path.dirname(name))
			tarinfo = tarfile.TarInfo(name)
			tarinfo.type = tarfile.DIRTYPE
			tarinfo.mtime = int(time.time())
			tar.addfile(tarinfo)

	# Copy everything into it.
	for (path, target) in env['FILES']:
		while path[0] == '/':
			path = path[1:]
		make_dir(os.path.dirname(path))
		tarinfo = tar.gettarinfo(str(target), path)
		tarinfo.uid = 0
		tarinfo.gid = 0
		tarinfo.uname = "root"
		tarinfo.gname = "root"
		tar.addfile(tarinfo, file(str(target)))
	for (path, target) in env['LINKS']:
		while path[0] == '/':
			path = path[1:]
		make_dir(os.path.dirname(path))
		tarinfo = tarfile.TarInfo(path)
		tarinfo.type = tarfile.SYMTYPE
		tarinfo.linkname = target
		tarinfo.mtime = int(time.time())
		tar.addfile(tarinfo)

	# Add in extra stuff from the directory specified in the configuration.
	if len(config['EXTRA_FSIMAGE']) > 0:
		cwd = os.getcwd()
		os.chdir(config['EXTRA_FSIMAGE'])
		for f in glob.glob('*'):
			tar.add(f)
		os.chdir(cwd)

	tar.close()
	return 0
def fs_image_emitter(target, source, env):
	# We must depend on every file that goes into the image.
	deps = [f for (p, f) in env['FILES']]
	return (target, source + deps)
fs_image_builder = Builder(action = Action(fs_image_func, '$GENCOMSTR'), emitter = fs_image_emitter)

# Function to generate an ISO image.
def iso_image_func(target, source, env):
	config = env['_CONFIG']

	fsimage = str(source[-1])
	cdboot = str(env['CDBOOT'])
	loader = str(env['LOADER'])
	#kernel = str(env['KERNEL'])

	# Create the work directory.
	tmpdir = tempfile.mkdtemp('.infiiso')
	os.makedirs(os.path.join(tmpdir, 'boot'))
	os.makedirs(os.path.join(tmpdir, 'infi', 'modules'))

	# Copy stuff into it.
	#shutil.copy(kernel, os.path.join(tmpdir, 'infi'))
	shutil.copy(fsimage, os.path.join(tmpdir, 'infi', 'modules'))
	#for mod in env['MODULES']:
	#	shutil.copy(str(mod), os.path.join(tmpdir, 'infi', 'modules'))

	# Write the configuration.
	f = open(os.path.join(tmpdir, 'boot', 'loader.cfg'), 'w')
	f.write('set "timeout" 5\n')
	f.write('entry "Infinity OS" {\n')
	if len(config['FORCE_VIDEO_MODE']) > 0:
		f.write('   set "video_mode" "%s"\n' % (config['FORCE_VIDEO_MODE']))
	f.write('   laos "/infi/kernel" "/infi/modules"\n')
	f.write('}\n')
	f.close()

	# Create the loader by concatenating the CD boot sector and the loader
	# together.
	f = open(os.path.join(tmpdir, 'boot', 'cdboot.img'), 'w')
	f.write(open(cdboot, 'r').read())
	f.write(open(loader, 'r').read())
	f.close()

	# Create the ISO.
	verbose = (ARGUMENTS.get('V') == '1') and '' or '>> /dev/null 2>&1'
	if os.system('mkisofs -J -R -l -b boot/cdboot.img -V "Infi CDROM" ' + \
			'-boot-load-size 4 -boot-info-table -no-emul-boot ' + \
			'-o %s %s %s' % (target[0], tmpdir, verbose)) != 0:
		print "Could not find mkisofs! Please ensure that it is installed."
		shutil.rmtree(tmpdir)
		return 1

	# Clean up.
	shutil.rmtree(tmpdir)
	return 0
def iso_image_emitter(target, source, env):
	assert len(source) == 1
	#return (target, [env['KERNEL'], env['LOADER'], env['CDBOOT']] + env['MODULES'] + source)
	return (target, [env['LOADER'], env['CDBOOT']] + source)
iso_image_builder = Builder(action = Action(iso_image_func, '$GENCOMSTR'), emitter = iso_image_emitter)

#EOF
