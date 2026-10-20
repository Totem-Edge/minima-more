# Node Command & Tool Reference

<!-- GENERATED from the node capability manifest (RFC-0011 §5) - do not edit by hand. -->
<!-- Regenerate with tests/gen_command_reference; tests/test_docs_manifest asserts it matches. -->

Node: version `1.0.100.0`, network `0x00`, mode `full`.

Commands: 111.

## backup

| Command | Access | Parameters | Summary |
|---|---|---|---|
| `archive` | destructive | `action`, `host`, `phrase`, `anyphrase`, `keys`, `keyuses`, `file`, `address`, `statecheck`, `logs`, `maxexport` | [action:] (host:) (phrase:) (keys:) (keyuses:) - Resync your chain with seed phrase if necessary (otherwise wallet remains the same) |
| `backup` | destructive | `debug`, `password`, `file`, `auto`, `confirm`, `maxhistory` | (password:) (file:) (auto:) (maxhistory:) - Backup the system. Uses a timestamped name by default |
| `coinexport` | destructive | `coinid` | [coinid:] - Export a coin |
| `coinimport` | destructive | `data`, `track` | [data:] (track:true\|false) - Import a coin, and keep tracking it |
| `megammr` | destructive | `action`, `file` | (action:) (file:) - Get Info on or Import / Export the MegaMMR data |
| `megammrsync` | destructive | `action`, `host`, `phrase`, `anyphrase`, `keys`, `keyuses`, `file`, `password` | [action:] [host:] (phrase:) (keys:) (keyuses:) (file:) (password:) - Restore from a MegaMMR node |
| `reset` | destructive | `action`, `archivefile`, `file`, `password`, `phrase`, `keys`, `keyuses` | [archivefile:] [action:] (file:) (password:) (keys:) (keyuses:) - Reset the entire system using an Archive Backup file... |
| `restore` | destructive | `file`, `password`, `shutdown` | [file:] (password:) - Restore the entire system. |
| `restoresync` | destructive | `file`, `password`, `host` | [file:] (password:) (host:) (keyuses:) - Restore the entire system AND perform an archive sync. Use when the backup is old. |
| `vault` | destructive | `action`, `seed`, `keyuses`, `phrase`, `password`, `confirm`, `numkeys` | [action:seed\|wipekeys\|restorekeys\|passwordlock\|passwordunlock] (seed:) (phrase:) - BE CAREFUL. Wipe/Restore/Encrypt/Decrypt your private keys |

## base

| Command | Access | Parameters | Summary |
|---|---|---|---|
| `automine` | destructive | - | [enable:true\|false\|single] - Simulate traffic |
| `block` | read | - | Simply return the current top block |
| `burn` | destructive | - | View Burn metrics |
| `capabilities` | read | `markdown` | Get the node capability manifest (commands, params, annotations) |
| `checkaddress` | read | `address` | [address:] - Check an address is valid |
| `coincheck` | destructive | `data` | [data:] - Check a coin exists |
| `coinnotify` | destructive | `action`, `address` | [action:] [address:] - listen for a specific coin address and send a NOTIFYCOIN message when found in chain |
| `cointrack` | destructive | `enable`, `coinid` | [enable:true\|false] [coinid:] - Track or untrack a coin |
| `consolidate` | destructive | `tokenid`, `coinage`, `maxcoins`, `maxsigs`, `burn`, `debug`, `dryrun`, `password` | [tokenid:] (coinage:) (maxcoins:) (maxsigs:) (burn:) (debug:) (dryrun:) - Consolidate coins by sending them back to yourself |
| `consolidatefrom` | destructive | `fromaddress`, `tokenid`, `script`, `privatekey`, `keyuses`, `mine`, `burn`, `maxcoins` | [fromaddress:] [address:] [amount:] (tokenid:) [script:] [privatekey:] [keyuses:] (burn:) (mine:) - Send Minima or Tokens from a certain address |
| `constructfrom` | destructive | `coinlist`, `script`, `toaddress`, `toamount`, `changeaddress`, `changeamount`, `tokenid` | [coinlist:] [script:] [toaddress:] [toamount:] [changeaddress:] [changeamount:] (tokenid:) - Create unsigned txn from a list of coins |
| `convert` | read | `from`, `to`, `data` | [from:] [to:] [data:] - Convert between different data types (String, HEX, Mx, Base64) |
| `createfrom` | destructive | `fromaddress`, `address`, `amount`, `tokenid`, `script`, `burn` | [fromaddress:] [address:] [amount:] (tokenid:) [script:] (burn:) - Create unsigned txn from a certain address |
| `debugflag` | destructive | `activate`, `var` | (activate:true\|false) (var:) - Set DEBUG flag for testing code.. |
| `decryptbackup` | destructive | `file`, `password`, `output` | [file:] (password:) (output:)- Decrypt an encrypted backup. |
| `hash` | read | `data`, `type`, `file` | (data:) (file:) (type:sha2\|sha3)- Hash the data - default SHA3 |
| `hashtest` | destructive | `amount` | (amount:) - Check the speed of hashing of this device. Defaults to 1 million hashes |
| `healthcheck` | read | - | Run a system check to see everything adds up |
| `help` | read | `command` | Show Help. [] are required. () are optional. Use 'help command:' for full help. Chain multiple commands with ; |
| `history` | read | `depth`, `max`, `offset`, `action`, `relevant`, `startmilli`, `where` | (action:) (max:) (offset:) (relevant:) - Search for all relevant TxPoW |
| `incentivecash` | destructive | `uid` | (uid:) - Show your rewards or specify your UserID for the Incentive Cash program |
| `keys` | destructive | `action`, `publickey`, `phrase`, `modifier` | (action:list\|new\|checkkeys\|genkey) (publickey:) - Get a list of all your public keys or create a new key |
| `logs` | destructive | `scripts`, `mining`, `maxima`, `blocks`, `networking`, `ibd`, `peerschecker`, `txpowdb` | (scripts:) (mining:) (maxima:) (networking:) (blocks:) (ibd:) (txpowdb:) - Enable full logs for various parts of Minima |
| `maths` | read | `calculate`, `logs` | [calculate:] (logs:)- Run maths with Minima precision MiniNumber |
| `mempool` | read | - | Check the mempool |
| `memreport` | read | - | Live memory breakdown (process, databases, in-RAM structures) |
| `mmrcreate` | destructive | `nodes` | [nodes:[]] - Create an MMR Tree of data. Nodes can be STRING / HEX |
| `mmrproof` | destructive | `data`, `proof`, `root` | [data:] [proof:] [root:] - Check an MMR proof |
| `multisig` | destructive | `id`, `action`, `root`, `required`, `file`, `publickeys`, `amount`, `tokenid`, `coinid`, `address`, `password`, `mine` | Create a multisig coin that can be used by root OR n of m txns |
| `multisigread` | destructive | `id`, `action`, `root`, `required`, `file`, `publickeys`, `amount`, `tokenid`, `coinid`, `address`, `password` | View, list or start a spend of a multisig coin that can be used by root OR n of m txns |
| `mysql` | destructive | `action`, `host`, `database`, `user`, `password`, `keys`, `keyuses`, `phrase`, `address`, `txpowid`, `enable`, `file`, `statecheck`, `logs`, `maxexport`, `readonly`, `startfix`, `endfix`, `block` | Store and resync the archive data in a MySQL database |
| `mysqlcoins` | destructive | `action`, `host`, `database`, `user`, `password`, `logs`, `readonly`, `query`, `where`, `maxblocks`, `maxcoins`, `hidetoken`, `address`, `spent`, `limit`, `enable` | Create and search a coins database from your MySQL Archive |
| `newscript` | destructive | `script`, `trackall`, `clean` | [script:] [trackall:false\|true] (clean:false\|true) - Add a new custom script. Track ALL addresses or just ones with relevant state variables. |
| `p2pstate` | destructive | - | prints full details of the internal p2p state |
| `postfrom` | destructive | `data`, `mine`, `mmr` | [data:] (mine:true\|false) (mmr:true\|false) - Post a signfrom txn  |
| `printmmr` | read | - | Print the MMR set of the tip block |
| `printtree` | read | `depth`, `cascade` | (depth:) (cascade:true\|false) - Print a tree representation of the blockchain. Depth default 32, Cascade false. |
| `quit` | destructive | `compact` | (compact:) - Shutdown Minima. Compact the Databases if you want |
| `random` | destructive | `size`, `type` | (size:) (type:) - Generate a random hash value, defaults to 32 bytes |
| `rawfrom` | destructive | `inputs`, `outputs`, `state` | [inputs:] [outputs:] (state:) - Create an unsigned transaction from a set of inputs, outputs and state |
| `removescript` | destructive | `address` | [address:] - Remove a script from your DB |
| `rpc` | destructive | `enable`, `ssl`, `password`, `action`, `username`, `password`, `mode` | (enable:) (ssl:) (password:) - Enable and disable RPC on port 9005 (default is off) |
| `runscript` | destructive | `script`, `state`, `prevstate`, `globals`, `signatures`, `extrascripts` | [script:] (state:{}) (prevstate:{}) (globals:{}) (signatures:[]) (extrascripts:{}) - Run a script with the defined parameters |
| `scanchain` | destructive | `depth` | (depth:) - Scan back through the chain and see all transaction data |
| `scripts` | destructive | `address` | (address:) - Search scripts / addresses |
| `seedrandom` | destructive | `modifier` | [modifier:] - Generate a random value, based on your SEED and a modifier |
| `sign` | destructive | `publickey`, `data` | [publickey:] [data:] - Sign the data with the publickey |
| `signfrom` | destructive | `id`, `data`, `privatekey`, `keyuses` | [data:] [privatekey:] [keyuses:] - Sign a creatfrom txn |
| `slavenode` | destructive | `host`, `enable` | Run in slavenode mode. Requires a master node to connect to. |
| `status` | read | `clean`, `debug`, `complete` | (clean:true) - Show general status for Minima and clean RAM |
| `systemcheck` | read | `processor`, `action` | Check system processors.. |
| `test` | destructive | `show`, `action` | test Funxtion |
| `timemilli` | read | `minutesback`, `hoursback` | Returns the current time in milliseconds |
| `tokencreate` | destructive | `name`, `amount`, `decimals`, `script`, `state`, `signtoken`, `webvalidate`, `burn`, `mine`, `uselimits` | [name:] [amount:] (decimals:) (script:) (state:{}) (signtoken:) (webvalidate:) (burn:) - Create a token. 'name' can be a JSON Object |
| `tokens` | read | `tokenid`, `action`, `data` | (tokenid:) (action:import\|export) (data:) - List, import or export tokens on the chain |
| `tokenvalidate` | destructive | `tokenid` | [tokenid:] - validate the signature and web link in a token |
| `trace` | read | `enable`, `filter`, `network` | [enable:true\|false] (filter:) (network:) - Show the message stacks of the internal Minima Engine with optional filter string. Only works on terminal. |
| `tutorial` | destructive | - | Show the complete Grammar for Minima KISSVM scripting |
| `txpow` | read | `txpowid`, `block`, `address`, `onchain`, `relevant`, `max`, `action`, `inblock` | (txpowid:) (onchain:) (block:) (address:) (relevant:) (max:) - Search for a specific TxPoW or check for onchain |
| `verify` | destructive | `publickey`, `data`, `signature` | [publickey:] [data:] [signature:] - Verify a signature |
| `webhooks` | destructive | `enable`, `action`, `hook`, `filter` | (action:list\|add\|remove\|clear) (hook:url) - Add a web hook that is called with Minima events as they happen |
| `whitepaper` | read | - | Print out the Minima White Paper |

## network

| Command | Access | Parameters | Summary |
|---|---|---|---|
| `connect` | destructive | `host` | [host:ip:port] - Connect to a network Minima instance |
| `disconnect` | destructive | `uid` | [uid:uid] - Disconnect from a connected or connecting host |
| `message` | destructive | `uid`, `data` | [data:message] (uid:uid) - Send a message over the network to one of your direct peers |
| `network` | read | `action` | (action:list\|reset\|recalculateip) - Show network status or reset traffic counter |
| `peers` | destructive | `action`, `peerslist`, `max`, `file`, `url`, `trust` | (action:list\|addpeers) (peerslist:) (trust:) - List or add peers. trust:true = force connect (known-good peers only) |
| `ping` | read | `host` | [host:] - Ping a host and get back Minima Node info |

## send

| Command | Access | Parameters | Summary |
|---|---|---|---|
| `send` | destructive | `action`, `uid`, `address`, `amount`, `multi`, `tokenid`, `state`, `burn`, `coinage`, `split`, `debug`, `dryrun`, `mine`, `password`, `storestate`, `fromaddress`, `signkey` | (address:Mx..\|0x..) (amount:) (multi:[address:amount,..]) (tokenid:) (state:{}) (password:) (burn:) (split:) (coinage:) (mine:) (debug:) (dryrun:) - Send Minima or Tokens to an address |
| `sendfrom` | destructive | `fromaddress`, `address`, `amount`, `tokenid`, `script`, `privatekey`, `keyuses`, `mine`, `burn`, `state` | [fromaddress:] [address:] [amount:] (tokenid:) (state:) [script:] [privatekey:] [keyuses:] (burn:) (mine:) - Send Minima or Tokens from a certain address |
| `sendnosign` | destructive | `address`, `amount`, `multi`, `tokenid`, `state`, `burn`, `split`, `debug`, `dryrun`, `file`, `coinage` | (address:Mx..\|0x..) (amount:) (multi:[address:amount,..]) (tokenid:) (state:{}) (burn:) (split:) (file:) (debug:) - Create a txn but don't sign it |
| `sendpoll` | destructive | `action`, `uid`, `address`, `amount`, `multi`, `tokenid`, `state`, `burn`, `coinage`, `split`, `debug`, `dryrun`, `mine`, `password`, `storestate`, `fromaddress`, `signkey` | (action:add\|list\|remove) (uid:) - Send function that is added to a list and polls until complete |
| `sendpost` | destructive | `file` | [file:] - Post a signed txn |
| `sendsign` | destructive | - | [file:] - Sign a previously created txn |
| `sendview` | destructive | `file` | [file:] - View a transaction ( signed or unsigned ) |

## txn

| Command | Access | Parameters | Summary |
|---|---|---|---|
| `txnaddamount` | destructive | - | [id:] [amount:] (address) (onlychange:) (tokenid:) (burn:) - Add inputs and calculate change for a certain amount |
| `txnauto` | destructive | `id`, `amount`, `address`, `tokenid`, `sign`, `burn`, `mmrscript` | [id:] [amount:] [address:] (tokenid:) (sign:) (burn:) (mmrscript:) - Create a transaction automatically |
| `txnbasics` | destructive | `id` | [id:] - Automatically set the MMR proofs and scripts for a txn |
| `txncheck` | destructive | `id` | [id:] - Show details about the transaction |
| `txnclear` | destructive | `id`, `scripts`, `mmr`, `signatures` | [id:] (scripts:) (mmr:) (signatures:) - Clear the Witness data |
| `txncoinlock` | destructive | `action` | (action:) - Lock coins used in un-broadcast transactions |
| `txncreate` | destructive | `id` | [id:] - Create a transaction |
| `txndelete` | destructive | `id` | [id:] - Delete this custom transaction |
| `txnexport` | destructive | `id`, `file`, `showtxn` | [id:] (file:) - Export a transaction as HEX or to a file |
| `txnimport` | destructive | `id`, `file`, `data` | (id:) (file:) (data:) - Import a transaction as a file or HEX data. Optionally specify the ID |
| `txninput` | destructive | - | [id:] (coinid:) (coindata:) (floating:) (address:) (amount:) (tokenid:) (scriptmmr:true)- Add a coin as an input to a transaction |
| `txnlist` | destructive | `id`, `transactiononly` | (id:) (transactiononly:) - List current custom transactions |
| `txnlock` | destructive | `action`, `timeout`, `unlockdelay` | (action) (timeout) (unlockdelay) - Gain a lock. Stops multiple txnfunctions occuring simultaneously |
| `txnmine` | destructive | `id`, `data` | (id:) (data:) - Mine a txn but don't post it from either ID or txnexport Data |
| `txnminepost` | destructive | `data` | [data:] - Post a pre-mined transaction |
| `txnmmr` | destructive | `id` | [id:] - Add MMR proofs to a transaction leaving those already present |
| `txnoutput` | destructive | `id`, `amount`, `address`, `tokenid`, `storestate` | [id:] [amount:] [address:] (tokenid:) (storestate:) - Create a transaction output |
| `txnpost` | destructive | `id`, `auto`, `burn`, `mine`, `txndelete` | [id:] (auto:true) (burn:) (mine:) (txndelete:)- Post a transaction. Automatically set the Scripts and MMR |
| `txnscript` | destructive | `id`, `scripts`, `auto` | [id:] (auto:false) (scripts:{}) - Add scripts to a txn |
| `txnsign` | destructive | `id`, `publickey`, `txndelete`, `txnpostauto`, `txnpostburn`, `txnpostmine`, `password`, `privatekey`, `keyuses` | [id:] [publickey:0x..\|auto] (txnpostauto:) (txnpostburn:) (txnpostmine:) (txndelete:) - Sign a transaction |
| `txnstate` | destructive | `id`, `port`, `value` | [id:] [port:] [value:] - Add a state variable |
| `txnview` | destructive | `file`, `data` | (file:) (data:) - View a transaction as a JSON. |

## wallet

| Command | Access | Parameters | Summary |
|---|---|---|---|
| `balance` | read | `address`, `tokenid`, `confirmations`, `tokendetails`, `megammr`, `simple` | (address:) (tokenid:) (confirmations:) (megammr:) - Show your total balance of Minima and tokens |
| `coins` | read | - | (relevant:true) (sendable:true) (coinid:) (amount:) (address:) (tokenid:) (coinage:) (checkmempool:) (order:) - Search for coins |
| `getaddress` | read | - | Get one of your default Minima addresses |
| `newaddress` | destructive | - | Create a new address that will not be not used for anything else (not a default change address) |

