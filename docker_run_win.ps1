# Set the display variable for X server
$env:DISPLAY="host.docker.internal:0.0"

# Run the Docker container
docker run -it --rm --privileged -e DISPLAY=$env:DISPLAY -v ${PWD}:/workspace --device /dev/bus/usb --device /dev/snd --device /dev/dri -w /workspace --entrypoint /bin/bash kazawai/gr-lora_utilities:latest