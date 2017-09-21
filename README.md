# dbase-pecl-php7

__Please use the official version at https://pecl.php.net/package/dbase , this github page was only made as a stopgap solution while there was no official pecl dbase version for php7. The pecl version is newer and has more bugfixes. This github repo should be considered deprecated__

 php7 compatibility.
Sources: 
 https://wiki.php.net/phpng-upgrading
 
 https://github.com/zxcvdavid/php-memcached/commit/e3dc831f1cb3e851c9e3db38c495358844bbc13b
 
 the php source code
 
 pspell source code
 
Everything seems to work, except updating (and probably inserting) with arrays that contain named indexes, as seen in the example here https://secure.php.net/manual/en/function.dbase-replace-record.php I have a suspicion this did never work.

With a raw array it works.

Installation instructions:
```
# PHP 7
sudo add-apt-repository ppa:ondrej/php
sudo apt-get install  php7.0-fpm php7.0-curl php7.0-mysql php7.0-dev 

# dbase for PHP 7
git clone git://github.com/mote0230/dbase-pecl-php7.git ~/php7-dbase
cd php7-dbase/
phpize
./configure
make
sudo make install
cd ~
rm -rf ~/php7-dbase

# load extension way 1
touch /etc/php/7.0/mods-available/dbase.ini
echo "extension=dbase.so" | tee -a /etc/php/7.0/mods-available/dbase.ini
ln -s /etc/php/7.0/mods-available/dbase.ini /etc/php/7.0/fpm/conf.d/20-dbase.ini
ln -s /etc/php/7.0/mods-available/dbase.ini /etc/php/7.0/cli/conf.d/20-dbase.ini

# or load extension way 2
echo "extension=dbase.so" | tee -a /etc/php/7.0/cli/php.ini
echo "extension=dbase.so" | tee -a /etc/php/7.0/fpm/php.ini

# restart
service php7.0-fpm restart
```
