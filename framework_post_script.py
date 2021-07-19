
import os
import shutil
import subprocess

if __name__ == "__main__":

	if 'EXIT_AT_START_POST' in os.environ and os.environ['EXIT_AT_START_POST'] == "TRUE":
		exit(0)
		
	print "Remove old copied framework and then copy final framework product to framework folder"

	folder = os.environ['PROJECT_FILE_PATH'] + "/../../Framework/" + os.environ['PRODUCT_NAME'] + ".framework"


	if os.path.isdir(folder):
	
		removeDirectoryCall = subprocess.Popen(['rm', '-r', folder], stdout=subprocess.PIPE)
		removeDirectoryData = removeDirectoryCall.communicate()[0]
		removeDirectoryCallReturnCode = removeDirectoryCall.returncode
	
	if os.path.isdir(os.environ['TARGET_BUILD_DIR'] + "/" + os.environ['PRODUCT_NAME'] + ".framework/"):
		copyDirectoryCall = subprocess.Popen(['cp', '-a', os.environ['TARGET_BUILD_DIR'] + "/" + os.environ['PRODUCT_NAME'] + ".framework/", os.environ['PROJECT_FILE_PATH'] + "/../../Framework/" + os.environ['PRODUCT_NAME'] + ".framework/"], stdout=subprocess.PIPE)
		copyDirectoryData = copyDirectoryCall.communicate()[0]
		copyDirectoryCallReturnCode = copyDirectoryCall.returncode

	if "OpenEarsDistribution/OpenEars" in os.environ['PROJECT_DIR']:
		if os.path.exists(os.environ['PROJECT_DIR'] + '/Dependencies/cmuclmtk/src/libs/autosih.h'):
			os.remove(os.environ['PROJECT_DIR'] + '/Dependencies/cmuclmtk/src/libs/autosih.h')