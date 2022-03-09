# Cuse Water

Cuse Water is a web site that allows you to view information about the different kinds of water pipes in Syracuse, NY, such as what material a pipe is made out of or the last time the pipe was serviced. This information is gotten from the [Open Data Syracuse](https://data.syrgov.net) API.

## Installation

```
$ git clone https://github.com/asoberan/cusewater.git
$ cd cusewater/
$ source env/bin/activate
(env) $ python3 -m pip install -r requirements.txt
(env) $ flask run
```

The site should now be available at http://127.0.0.1:5000
