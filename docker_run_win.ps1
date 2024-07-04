# Set the display variable for X server
$env:DISPLAY="host.docker.internal:0.0"

# Run the Docker container
docker run -it --rm --privileged -e DISPLAY=$env:DISPLAY -v ${PWD}:/workspace -w /workspace --entrypoint /bin/bash kazawai/gr-lora_utilities:latest