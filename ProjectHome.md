> To play with this connection\_limit module first compile it into a
> DSO file and install it into Apache's modules directory
> by running:

> $ apxs -c -i mod\_connection\_limit.c

> Then activate it in Apache's httpd.conf file for instance
> for the URL /connection\_limit in as follows:

  1. httpd.conf
```
    LoadModule connection_limit_module modules/mod_connection_limit.so
    <Location /connection_limit>
    SetHandler connection_limit
    </Location>
```
> Then after restarting Apache via

> $ apachectl restart

> you immediately can request the URL /connection\_limit and watch for the
> output of this module. This can be achieved for instance via:

> $ lynx -mime\_header http://localhost/connection_limit

> The output should be similar to the following one:

> HTTP/1.1 200 OK
> Date: Tue, 31 Mar 1998 14:42:22 GMT
> Server: Apache/1.3.4 (Unix)
> Connection: close
> Content-Type: text/html