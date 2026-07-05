<?php
	if (!isset($_GET["hash"]) || $_SERVER["HTTP_USER_AGENT"] != "PCMDS_Updater") die();
	
	$folder = "css34/";
	if (isset($_GET["engine"])) {
		switch($_GET["engine"]) {
			case "css34": 
				$folder = "css34/";
				break;
			case "css":
				$folder = "css/";
				break;
		}
	}
	
	
	$folder .= isset($_GET["experimental"]) ? "experimental/" : "stable/";
	$extension = "protectcmds.ext.so";
	
	$file = fopen($folder.$extension, "r");
	if (!$file) die();
	$contents = fread($file, filesize($folder.$extension));
	$hash = hash("sha256", $contents);
	if ($hash != $_GET["hash"]) {
		header("Content-Type: application/octet-stream");
		echo "ok".$contents;
	}
	else echo "updated";
	
	/*$fp = fopen("access.log", "a");
	if (!$fp) die();
	
	$query = date("Y-m-d H:i:s") . " " . $_SERVER["REMOTE_ADDR"] . " GET: " . $folder . " ANSWER: " . $answer;

	fwrite($fp, $query . PHP_EOL);
	fclose($fp);*/
?>
