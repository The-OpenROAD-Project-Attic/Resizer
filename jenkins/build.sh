docker build -f jenkins/Dockerfile.dev -t resizer .
docker run -v $(pwd):/resizer resizer bash -c "./resizer/jenkins/install.sh"