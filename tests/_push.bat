ck add repo:remote-ck-node --url=http://localhost:3333 --remote --quiet
start ..\build\ck-crowdnode-server
ck push remote-ck-node:: --filename=ck-master.zip
