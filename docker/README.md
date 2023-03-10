# Building the image
``` sh
docker build -t "chogori-builder-sql" .
```

# Building and installing Chogori-SQL
## Run the container from the image we built above
This mounts your host current directory `PWD` into the `/host` directory of the container. Any changes you do in the container inside the `/host` dir
ectory are reflected into the host (much like a symlink)
``` sh
docker run -it --rm --init -v ${PWD}:/host chogori-builder-sql
```
## And now run the sql build and installation steps inside this container
``` sh
cd /host
git clone https://github.com/futurewei-cloud/chogori-sql.git
cd chogori-sql
mkdir -p build
cd build
cmake ../
make -j
```
## Running the integration tests
``` sh
cd /host/chogori-sql
test/integration/integrate.sh
```
# Running the regression tests
``` sh
test/regression.sh
```
# Starting the Chogori-SQL environment
``` sh
cd /host/chogori-sql/pgtest
./initDB.sh
./pg_run.sh &
```
# Running the sql client
``` sh
/host/chogori-sql/src/k2/postgres/bin/psql postgres
```
