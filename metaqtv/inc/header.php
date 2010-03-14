<!DOCTYPE html PUBLIC '-//W3C//DTD XHTML 1.0 Transitional//EN' 'http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd'>
<html xmlns='http://www.w3.org/1999/xhtml' xml:lang='en' lang='en'>
<head>
  <meta http-equiv="Content-type" content="text/html; charset=utf-8" />
  <title>QuakeTV: Now Playing</title>
  <link rel="StyleSheet" href="style.css" type="text/css" />
  <link rel="StyleSheet" href="<?=TOPBAR_CSS?>" type="text/css" />
  <link rel="alternate" type="application/rss+xml" title="RSS" href="?rss" />
</head>
<body>
<?php @include(TOPBAR_ADDR); ?>
<h1>QuakeTV</h1>
<p id="subtitle">Live broadcasts from all around the world<br />
<a href=".">Active only</a>
| <a href="?full">Full List</a>
| <a href="?rss&amp;limit=1">RSS</a>
| <a href="aliases.php">Server Aliases</a></p>
<table id='nowplaying' cellspacing='0'>
