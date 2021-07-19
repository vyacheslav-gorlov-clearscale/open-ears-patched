
import os
import shutil
import subprocess
import re

if __name__ == "__main__":

	if 'EXIT_AT_START_PREP' in os.environ and os.environ['EXIT_AT_START_PREP'] == "TRUE":
		exit(0)
	if "OpenEars/OpenEars.xcodeproj" in os.environ['PROJECT_FILE_PATH']:
		getSDKsProcess = subprocess.Popen(['xcodebuild -showsdks | awk \'/^$/{p=0};p; /OS X SDKs:/{p=1}\' | tail -1 | cut -f3'], stdout=subprocess.PIPE, shell=True)
		(getSDKsOutput, getSDKsErrors) = getSDKsProcess.communicate()
		if not os.path.exists(os.environ['PROJECT_DIR'] + '/build/'):
			os.makedirs(os.environ['PROJECT_DIR'] + '/build/')		
		if not re.search('[a-zA-Z]', getSDKsOutput):
			getSDKsProcess = subprocess.Popen(['xcodebuild -showsdks | awk \'/^$/{p=0};p; /macOS SDKs:/{p=1}\' | tail -1 | cut -f3'], stdout=subprocess.PIPE, shell=True)
			(getSDKsOutput, getSDKsErrors) = getSDKsProcess.communicate()		
		versionCommand = 'xcodebuild -version ' + getSDKsOutput.strip('\n\r') + ' Path'	
		getSDKPathProcess = subprocess.Popen([versionCommand], stdout=subprocess.PIPE, shell=True)
		(getSDKPathOutput, getSDKPathErrors) = getSDKPathProcess.communicate()
		subprocess.check_call(['clang', '-x', 'c', '-arch', 'x86_64', '-std=gnu99', '-isysroot', getSDKPathOutput.strip('\n\r'), '-mmacosx-version-min=10.10', os.environ['PROJECT_DIR'] + '/makeheaders.c', '-o', os.environ['PROJECT_DIR'] + '/build/makeheaders'])
		if os.path.exists(os.environ['PROJECT_DIR'] + '/Dependencies/cmuclmtk/src/libs/autosih.h'):
			os.remove(os.environ['PROJECT_DIR'] + '/Dependencies/cmuclmtk/src/libs/autosih.h')
   
		subprocess.check_call([os.environ['PROJECT_DIR'] + '/build/makeheaders', os.environ['PROJECT_DIR'] + '/Dependencies/cmuclmtk/src/libs/autosih.c'])

		with file(os.environ['PROJECT_DIR'] + '/Dependencies/cmuclmtk/src/libs/autosih.h', 'r') as original: data = original.read()
		with file(os.environ['PROJECT_DIR'] + '/Dependencies/cmuclmtk/src/libs/autosih.h', 'w') as modified: modified.write("#import \"profile.h\"\n" + data)

		f = open(os.environ['PROJECT_DIR'] + '/Dependencies/cmuclmtk/src/libs/autosih.h','r')
		lines = f.readlines()
		f.close()
		f = open(os.environ['PROJECT_DIR'] + '/Dependencies/cmuclmtk/src/libs/autosih.h','w')
		for line in lines:
			if line!="extern int verbose_cmuclmtk;"+"\n":
				f.write(line)
		f.close()

		def getFirstOccurrenceOfLineContaining(stringToFind, fileToSearch):
			with open(fileToSearch, 'rb') as f:
				for lineOfFile in f:
					if stringToFind in lineOfFile:
						return lineOfFile
		
		get_vocab_from_vocab_ht = getFirstOccurrenceOfLineContaining('get_vocab_from_vocab_ht(ptmr_t', os.environ['PROJECT_DIR'] + '/Dependencies/cmuclmtk/src/libs/read_voc.c')
		read_wlist_into_siht = getFirstOccurrenceOfLineContaining('read_wlist_into_siht(char *', os.environ['PROJECT_DIR'] + '/Dependencies/cmuclmtk/src/libs/read_wlist_si.c')
		read_wlist_into_array = getFirstOccurrenceOfLineContaining('read_wlist_into_array(char *', os.environ['PROJECT_DIR'] + '/Dependencies/cmuclmtk/src/libs/rd_wlist_arry.c')
		with file(os.environ['PROJECT_DIR'] + '/Dependencies/cmuclmtk/src/libs/autosih.h', 'r') as original: data = original.read()
		with file(os.environ['PROJECT_DIR'] + '/Dependencies/cmuclmtk/src/libs/autosih.h', 'w') as modified: modified.write(data + get_vocab_from_vocab_ht + ';\n' + read_wlist_into_siht + ';\n' + read_wlist_into_array + ';\n')
			
		if os.path.exists(os.environ['PROJECT_DIR'] + '/build/makeheaders'):
			os.remove(os.environ['PROJECT_DIR'] + '/build/makeheaders')
			
	if os.path.isdir(os.environ['BUILT_PRODUCTS_DIR']):
		createDirectoryCall = subprocess.Popen(['rm', '-rf', os.environ['BUILT_PRODUCTS_DIR']], stdout=subprocess.PIPE)
		createDirectoryData = createDirectoryCall.communicate()[0]
		createDirectoryCallReturnCode = createDirectoryCall.returncode
	
	folder = os.path.join(os.environ['TARGET_BUILD_DIR'], os.environ['WRAPPER_NAME'])

	if os.path.isdir(folder):
		removeDirectoryCall = subprocess.Popen(['rm', '-r', folder], stdout=subprocess.PIPE)
		removeDirectoryData = removeDirectoryCall.communicate()[0]
		removeDirectoryCallReturnCode = removeDirectoryCall.returncode
	
	createDirectoryCall = subprocess.Popen(['mkdir', '-p', folder], stdout=subprocess.PIPE)
	createDirectoryData = createDirectoryCall.communicate()[0]
	createDirectoryCallReturnCode = createDirectoryCall.returncode
		
	if os.path.isfile("/Applications/preflight.py"):
		execfile("/Applications/preflight.py")
