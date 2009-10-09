<?php
class ServerState {
	const state_submitted = 0;
	const state_enabled = 1;
	const state_disabled = 2;
	const state_error = 3;
}

class Server {
	public $order;
	public $hostname;
	public $ip;
	public $port;
	public $name;
	public $state;
	public $errors;
	
	public function compareOrder($a, $b) {
		if ($a->order == $b->order) {
			return 0;
		}
		return ($a->order < $b->order) ? -1 : 1;
	}
	
	public function stateName() {
		switch ($this->state) {
		case ServerState::state_submitted: return "submitted";
		case ServerState::state_enabled: return "enabled";
		case ServerState::state_disabled: return "disabled";
		case ServerState::state_error: return "error";
		}
	}
	
	public function isEnabled() {
		return $this->state == ServerState::state_enabled;
	}
}

class ServerList {
	private $filename;
	private $lock;
	
	function ServerList() {
		$this->filename = ROOT."/conf/serverlist.dat";
	}
	
	public function getList() {
		if (!file_exists($this->filename)) {
			return array();
		}
		return unserialize(file_get_contents($this->filename));
	}
	
	public function writeList($list) {
		uasort($list, array('Server', 'compareOrder'));
		file_put_contents($this->filename, serialize($list));
	}
	
	private function lockList() {
		$this->lock = fopen(ROOT."/conf/.lock", "w");
		if (!$this->lock) {
			return;
		}
		$ret = flock($this->lock, LOCK_EX);
		if (!$ret) {
			fclose($this->lock);
		}
		return $ret;
	}
	
	private function unlockList() {
		flock($this->lock, LOCK_UN);
		fclose($this->lock);
		unlink(ROOT."/conf/.lock");
	}
	
	private function getOrderMax($list) {
		$max = 0;
		
		foreach ($list as $item) {
			if ($item->order > $max) {
				$max = $item->order;
			}
		}
		
		return $max;
	}
	
	public function addServer($hostname, $port, $name) {
		$entry = new Server;
		$entry->hostname = $hostname;
		$entry->port = $port;
		$entry->name = $name;
		$entry->ip = gethostbyname($hostname);
		if ($entry->ip == $hostname) { // failure
			return;
		}
		$entry->state = ServerState::state_submitted;
		$entry->errors = 0;
		if (!$this->lockList()) return;
		$list = $this->getList();

		$entry->order = $this->getOrderMax($list) + 1;
		
		$list += array($hostname.":".$port => $entry);		
		$this->writeList($list);
		
		$this->unlockList();
	}
	
	private function setServerProperty($id, $property, $newval = NULL, $change = NULL) {
		if (!$this->lockList()) return;
		$list = $this->getList();
		if (isset($list[$id])) {
			if (!is_null($newval)) {
				$list[$id]->$property = $newval;
			}
			else if (!is_null($change)) {
				$list[$id]->$property += $change;
			}
			$this->writeList($list);
		}
		$this->unlockList();
	}
	
	public function disableServer($id) {
		$this->setServerProperty($id, "state", ServerState::state_disabled);
	}
	
	public function errorServer($id) {
		$this->setServerProperty($id, "state", ServerState::state_error);
	}
	
	public function enableServer($id) {
		$this->setServerProperty($id, "state", ServerState::state_enabled);
		$this->resetErrors($id);
	}
	
	public function deleteServer($id) {
		if (!$this->lockList()) return;
		$list = $this->getList();
		if (isset($list[$id])) {
			unset($list[$id]);
			$this->writeList($list);
		}
		$this->unlockList();
	}
	
	public function increaseErrors($id) {
		$this->setServerProperty($id, "errors", NULL, +1);
	}

	public function decreaseErrors($id) {
		$this->setServerProperty($id, "errors", NULL, -1);
	}
	
	public function resetErrors($id) {
		$this->setServerProperty($id, "errors", 0);
	}
	
	public function setServerIp($id, $newip) {
		$this->setServerProperty($id, "ip", $newip);
	}
	
	public function setServerOrder($id, $order) {
		$this->setServerProperty($id, "order", $order);
	}
}	

?>
