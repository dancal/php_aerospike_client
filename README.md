Changes
---------------------------------------------
add close function


Accessing Aerospike Database via PHP requires building the Aerospike PHP native extension and placing the shared library file in the php system location:

Building & Testing Aerospike PHP extension :
---------------------------------------------
1) make sure php-devel is installed
   # RHEL, CentOS
   sudo yum install glibc-devel openssl-devel php-devel

   # In some installations, these additional dependencies are also needed
   sudo yum install libxslt-devel libxml2-devel libpng-devel libjpeg-devel

   # Debian, Ubuntu
   sudo apt-get install php5 php5-cli php5-dev

2) cd to the base path of where the Aerospike PHP client package is untar'ed -- the path of where this README file is:
   e.g.  cd citrusleaf_client_php_2.0.23.85CP

3) Issue command:
         phpize
             # If you are in a system directory:
         sudo phpize

   This will create the necessary "configure" executable, needed below.

4) Please make sure "make", "gcc", "openssl-devel" are installed on your system:
   # RHEL, CentOS
   sudo yum install glibc-devel make openssl-devel
   # Debian, Ubuntu
   sudo apt-get install libc6-dev make libssl-dev

5) Issue command: 
        ./configure --enable-citrusleaf
            # or, if you are in a system directory
        sudo ./configure --enable-citrusleaf

    ### note, we're safe at O3, so this may be better: 
    ###                   ./configure CFLAGS='-O3 -g' --enable-citrusleaf

6) Issue command:
     make
         # or, in system locations
     sudo make

7) copy modules/citrusleaf.so and put it in your default php modules directory (eg "/usr/lib64/php/modules"):
     cp modules/citrusleaf.so /usr/lib64/php/modules

8) Use an editor to modify your php.ini (eg "/etc/php.ini") to include the citrusleaf.so
     enable_dl=On
     extension=citrusleaf.so

9) HTML format documentation of the Aerospike PHP extension is located in doc/citrusleaf_api.html.

10) Programming examples of using the PHP extension are in the examples directory. Please read examples/README for more details.



Statically linking citrusleaf client with PHP :
-----------------------------------------------

1) Please make sure "make", "gcc", "openssl-devel" are installed on your system:
    # RHEL, CentOS
    sudo yum install gcc make openssl-devel
    # Debian, Ubuntu
    sudo apt-get install gcc make libssl-dev

2) Download the desired version of php source code from php.net for eg:5.3.8 - php-5.3.8.tar.gz
    tar xzf php-5.3.8.tar.gz

3) Please start from a fresh php source as it was observed that sometime the configure does not get rebuilt properly when it was already built before, even when it was removed.

4) Copy the latest citrusleaf PHP client for eg:2.0.23.85 - citrusleaf_client_php_2.0.23.85CP.tg
    tar xzf citrusleaf_client_php_2.0.23.85CP.tg

    # Copy the client sources to PHP source
    cp -r citrusleaf_client_php_2.0.23.85CP php-5.3.8/ext/citrusleaf

5) Issue command
    cd php-5.3.8
    rm configure
    ./buildconf --force     # You may need to install an older autoconf version eg: autoconf 2.13 and then do "export PHP_AUTOCONF=autoconf-2.13" before running this

6) Run ./configure --enable-citrusleaf along with other PHP options that you need
     Eg: ./configure --enable-citrusleaf '--with-dom' '--with-dom-xslt' '--with-dom-exslt' '--enable-cgi' '--enable-ftp' '--with-gd' '--with-pear' '--with-zlib' '--with-kerberos' '--enable-fastcgi' '--enable-shmop' '--with-xsl'

7) Then do make and make install
    make
    sudo make install

8) The php & libphp*.so that are created will have citrusleaf libraries linked into it.
