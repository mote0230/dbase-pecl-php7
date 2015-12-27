# dbase-pecl-php7
 php7 compatibility.
Sources: 
 https://wiki.php.net/phpng-upgrading
 
 https://github.com/zxcvdavid/php-memcached/commit/e3dc831f1cb3e851c9e3db38c495358844bbc13b
 
 the php source code
 
 pspell source code
 
Everything seems to work, except updating (and probably inserting) with arrays that contain named indexes, as seen in the example here https://secure.php.net/manual/en/function.dbase-replace-record.php

With a raw array it works.
