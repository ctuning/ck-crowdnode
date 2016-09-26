rem call ck add repo:remote-ck-node --url=http://localhost:3333 --remote --quiet
rem start ..\build\ck-crowdnode-server
call ck push remote-ck-node:: --filename=ck-master.zip
