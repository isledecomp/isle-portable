#todo: actually make .desktop files
import sys
import subprocess
print("Script ony designed for Ubuntu. Made for the portable Lego Island project.")
def command(command):
    if isinstance(command, str):
        process = subprocess.Popen(
            command,
            shell=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1
        )
    else:
        process = subprocess.Popen(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1
        )

    # Print output line by line as it comes
    for line in process.stdout:
        print(line, end='')

    process.stdout.close()
    return_code = process.wait()
    if return_code != 0:
        print(f"\nCommand failed with return code {return_code}")
installdepscommand = b"sudo apt install -y \
libsdl3-0 libsdl3-dev \
libglew2.2 libglew-dev \
libstdc++6 \
libgcc-s1 \
libc6 \
libopengl0 \
libm6 \
libasound2 \
libx11-6 \
libxext6 \
libxcursor1 \
libxi6 \
libxfixes3 \
libxrandr2 \
libxss1 \
libpipewire-0.3-0 \
libpulse0 \
libsndio7.0 \
libdrm2 \
libgbm1 \
libwayland-egl1 \
libwayland-client0 \
libwayland-cursor0 \
libxkbcommon0 \
libdecor-0-0 \
libgl1 \
libglx0 \
libgldispatch0 \
libxcb1 \
libxrender1 \
libdbus-1-3 \
libbsd0 \
libexpat1 \
libffi8 \
libxau6 \
libxdmcp6 \
libsndfile1 \
libx11-xcb1 \
libsystemd0 \
libasyncns0 \
libapparmor1 \
libmd0 \
libflac8 \
libvorbis0a \
libvorbisenc2 \
libopus0 \
libogg0 \
libmpg123-0 \
libmp3lame0 \
libcap2 \
libmvec1 \
git"
if input("By pressing enter, you agree that all the dependencies will be installed using apt. If you do not consent, press CTRL+C. To see what command will be run, type \'command\'.") = "command":
	print("\n")
	print(installdepscommand)
	sys.exit()
else:
	print("\n")
print("Installing dependencies...\n")
command(installdepscommand)
print("Downloading latest release of Lego Island portable...\n")
command('''
REPO="isledecomp/isle-portable"
DEST_DIR="~"

mkdir -p "$DEST_DIR" && \
curl -s "https://api.github.com/repos/$REPO/releases/latest" | \
  grep "browser_download_url" | grep -i "linux" | cut -d '"' -f 4 | \
  while read -r url; do
    curl -L -o "$DEST_DIR/$(basename "$url")" "$url"
  done
''')
print("Extracting tarball...\n")
command('''tarball=$(ls ~/isle*.tar.gz | head -n 1); foldername="legoisle"; mkdir -p "$(dirname "$tarball")/$foldername" && tar -xzf "$tarball" -C "$(dirname "$tarball")/$foldername"
''')
print("Making desktop files...")
#make said desktop files
print("Done! YOU NEED TO RUN THIS COMMAND FOR LEGO ISLE TO WORK: sudo mv ~/legoisle/lib/liblego1.so /usr/lib/x86_64-linux-gnu/; sudo ldconfig \n")
