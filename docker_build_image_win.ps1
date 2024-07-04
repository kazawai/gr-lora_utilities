# Get the latest commit hash from the repository
$LATEST_VERSION = (git ls-remote https://github.com/kazawai/gr-lora_utilities.git | Select-String -Pattern 'HEAD').ToString().Split("`t")[0]

# Build the Docker image with the latest commit hash as the CACHEBUST argument
docker build -t kazawai/gr-lora_utilities --build-arg CACHEBUST=$LATEST_VERSION .