<?php

	define ("ADMIN_EMAIL", "");

	define ("DEBUG_MODE", false);
	define ("SOCKET_OPEN_TIMEOUT", (float) 1.5);
	define ("SOCKET_READ_TIMEOUT", (int) 2);
	
	// maximum failed connect attempts before server is disabled (state set to errored)
	define ("MAX_ERRORS", 30);
	
	// with this probability successfull connection will cause error counter decrease
	define ("ERROR_REDUCTION_COEF", 0.01); 
	
	define ("TOPBAR_ADDR", ROOT.'/qwnetwork/qwnetwork.php');
	define ("TOPBAR_CSS", 'http://network.quakeworld.nu/bar/style.css');
	
?>
