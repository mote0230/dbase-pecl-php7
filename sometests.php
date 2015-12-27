<html>
<body>
<?php
$db = dbase_open('/home/an existing dbf/existing.DBF', 0);
if ($db) {
echo('start ');
echo($db);
echo('enddb ');
echo('hi numrecords is: ');
echo(dbase_numrecords($db));
echo('\r\n Heres the header info:');
print_r(dbase_get_header_info($db));


echo('trying record num 2');
$rec = dbase_get_record($db, 2);
$nf  = dbase_numfields($db);
for ($i = 0; $i < $nf; $i++) {
  echo $rec[$i], "\n";
}




  dbase_close($db);
}
echo('end. Trying to create db...');

$def = array(
  array("date",     "D"),
  array("name",     "C",  50),
  array("age",      "N",   3, 0),
  array("email",    "C", 128),
  array("ismember", "L")
);

// creation
$db = dbase_create('/home/a writeable directory/test.dbf', $def);
if (!$db) {
  echo "Error, can't create the database\n";
} else {
echo('hi numrecords is: ');
echo(dbase_numrecords($db));
echo('\r\n Heres the header info:');
print_r(dbase_get_header_info($db));
echo('number of records: ');
echo(dbase_numrecords($db));
echo('inserting row...');
  dbase_add_record($db, array(
      date('Ymd'),
      'Maxim Topolov',
      '23',
      'max@example.com',
      'T'));
echo('finished inserting. number of records: ');
echo(dbase_numrecords($db));
echo('trying record num 1: ');
$rec = dbase_get_record($db, 1);
$nf  = dbase_numfields($db);
for ($i = 0; $i < $nf; $i++) {
  echo $rec[$i], "\n";
}

  dbase_add_record($db, array(
      date('Ymd'),
      'Maxi2222m Topolov',
      '23',
      'max@example.com',
      'T'));


echo('updating a row...');
  // gets the old row
  $row = dbase_get_record_with_names($db, 1);

  // remove the 'deleted' entry
  unset($row['deleted']);

$row =  array(
      date('Ymd'),
      'Maxitom',
      '23',
      'max@example.com',
      'T');

  // Update the date field with the current timestamp
//  $row['name'] = 'Tom';
echo('row before inserting is: ');
print_r($row);

  // Replace the record
  dbase_replace_record($db, $row, 1);

echo('trying record num 1: ');
$rec = dbase_get_record($db, 1);
$nf  = dbase_numfields($db);
for ($i = 0; $i < $nf; $i++) {
  echo $rec[$i], "\n";
}

echo('deleting row 2. before deletion: ');
$rec = dbase_get_record($db, 2);
$nf  = dbase_numfields($db);
for ($i = 0; $i < $nf; $i++) {
  echo $rec[$i], "\n";
}
echo( dbase_delete_record($db, 2));
echo('before db pack');
echo(dbase_pack($db));
echo('after pack, trying to fetch record 2');

$rec = dbase_get_record($db, 2);
$nf  = dbase_numfields($db);
for ($i = 0; $i < $nf; $i++) {
  echo $rec[$i], "\n";
}
  dbase_close($db);
}
?>


