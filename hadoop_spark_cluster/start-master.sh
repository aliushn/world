# !/bin/bash

na1="master"
docker rm -f $na1

docker  run  -itd --name $na1 -p 11230:11230 -p 11231:11231 -p 11232:11232  -p 11240:11240 -p 11241:11241 -p 11242:11242 -p 11243:11243 -p 11244:11244 -p 11245:11245 -p 11250:11250 -p 11251:11251 -p 11252:11252 -p 11260:11260  -p 11262:11262 -p 11270:11270 -p 11271:11271 -p 11272:11272 -h "master" -v /Users/cj/workspace/world/hadoop_spark_cluster/codes_and_data:/codes_data  bigdata_test_lichang_master:201901282105  /bin/bash
