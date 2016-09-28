#ck add repo:remote-ck-node --url=http://localhost:3333 --remote --quiet
#../build/ck-crowdnode-server &

ck shell remote-ck-node:: --filename=ck-master.zip  --secretkey=c4e239b4-8471-11e6-b24d-cbfef11692ca --cmd="ls -l /etc/"

