<?php

	// disable caching, from PHP manual
	header("Expires: Mon, 26 Jul 1997 05:00:00 GMT");
	header("Last-Modified: " . gmdate("D, d M Y H:i:s") . " GMT");
	header("Cache-Control: no-store, no-cache, must-revalidate");
	header("Cache-Control: post-check=0, pre-check=0", false);
	header("Pragma: no-cache");


	require "config.php";
	if (DEBUG_MODE) {
		error_reporting(E_ALL);
	}
	else {
		error_reporting(0);
	}
	
	$qtvlist = NULL;
	require "qtvlist.php";
	
	$cururl = "";
	$intable = false;
	$tablelev = 0;
	$ignoreline = false;
	$errors = "";
	
	function startElement($parser, $name, $attrs)
	{
		global $intable;
		global $tablelev;
		global $cururl;
		global $ignoreline;
		
		// we are entering a row which represents an empty server
		// so if user wants, we will set ignore flag on
		if ($name == "TR" && strpos(@$attrs["CLASS"], "notempty") === false && IGNORE_EMPTY && $tablelev == 1) {
			$ignoreline = true;
		}
		
		if ($ignoreline) {
			return;
		}
		
		if ($intable) {
			echo "<".$name;
			
			foreach($attrs as $k => $v) {
				if ($k == "HREF" || $k == "SRC") {
					if ($v[0] == "/") {
						$v = substr($v, 1);
					}
					$v = $cururl.$v;
				}
				echo ' '.htmlspecialchars($k).'="'.htmlspecialchars($v).'"';
			}
			
			echo ">";
		}
		
		if (!$intable && $name == "TABLE" && $attrs["ID"] == "nowplaying") {
			$intable = true;
		}
		
		if ($intable && $name == "TABLE") {
			$tablelev++;
		}		
	}
	
	function endElement($parser, $name)
	{
		global $intable;
		global $tablelev;
		global $ignoreline;
		
		if ($name == "TR") {
			if ($ignoreline) {
				$ignoreline = false;
				return;
			}
		}
		
		if ($ignoreline) {
			return;
		}
		
		if ($intable && $name == "TABLE") {
			$tablelev--;
			if (!$tablelev) {
				$intable = false;
			} 
		}
		
		if ($intable) {
			echo "</".$name.">";
		}
	}
	
	function cdata($parser, $d) {
		global $intable;
		global $ignoreline;
		
		if ($intable && !$ignoreline) {
			echo $d;
		}
	}
	
	function InsertUrl($url)
	{
		global $xml_parser;
		global $intable;
		global $tablelev;
		global $cururl;
		global $errors;

		$intable = false;
		$tablelev = 0;
		
		$cururl = $url;
		$fp = @fopen($url, "r");
		if (!$fp) {
			$errors .= "<p>Couldn't open {$url}</p>";
			return;
		}
		
		$xml_parser = xml_parser_create();
		if (!$xml_parser) {
			$errors .= "<p>Couldn't create XML parser</p>";
			return;
		}
		xml_parser_set_option($xml_parser, XML_OPTION_CASE_FOLDING, true);
		xml_set_element_handler($xml_parser, "startElement", "endElement");
		xml_set_character_data_handler($xml_parser, "cdata");
		
		while ($data = fread($fp, 4096)) {
    		if (!xml_parse($xml_parser, $data, feof($fp))) {
				break;
			}
		}
		
		xml_parser_free($xml_parser);
		fclose($fp);
	}

	include "header.html";
	
	foreach ($qtvlist as $url) {
		InsertURL($url);
	}

	if (DEBUG_MODE && strlen($errors)) {
		echo "</table>";
		echo "<div id='errors'>".$errors."</div><table>\n";
	}
	include "foot.html";

?>
