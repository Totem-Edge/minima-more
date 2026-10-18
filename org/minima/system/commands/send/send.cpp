#include "org/minima/system/commands/send/send.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/txpowdb/tx_po_w_d_b.hpp"
#include "org/minima/database/txpowtree/tx_pow_tree.hpp"
#include "org/minima/database/txpowtree/tx_po_w_tree_node.hpp"
#include "org/minima/database/userprefs/txndb/txn_row.hpp"
#include "org/minima/database/wallet/wallet.hpp"
#include "org/minima/database/wallet/script_row.hpp"

#include "org/minima/objects/address.hpp"
#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/token.hpp"
#include "org/minima/objects/transaction.hpp"
#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/objects/witness.hpp"
#include "org/minima/objects/state_variable.hpp"
#include "org/minima/objects/magic.hpp"

#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/base/mini_string.hpp"

#include "org/minima/objects/keys/signature.hpp"

#include "org/minima/system/main.hpp"
#include "org/minima/system/brains/tx_po_w_generator.hpp"
#include "org/minima/system/brains/tx_po_w_miner.hpp"
#include "org/minima/system/brains/tx_po_w_searcher.hpp"

#include "org/minima/system/commands/backup/vault.hpp"
#include "org/minima/system/commands/search/keys.hpp"
#include "org/minima/system/commands/txn/txnutils.hpp"
#include "org/minima/database/userprefs/txndb/txn_row.hpp"

#include "org/minima/system/params/general_params.hpp"
#include "org/minima/system/params/global_params.hpp"

#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/streamable.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

#include "org/minima/objects/coin_proof.hpp"
#include "org/minima/objects/script_proof.hpp"
#include "org/minima/objects/mmr/m_m_r_proof.hpp"
#include "org/minima/objects/mmr/m_m_r.hpp"
#include "org/minima/objects/mmr/m_m_r_entry_number.hpp"

using org::minima::database::MinimaDB;
using org::minima::database::txpowdb::TxPoWDB;
using org::minima::database::txpowtree::TxPoWTreeNode;
using org::minima::database::wallet::Wallet;

using org::minima::objects::Address;
using org::minima::objects::Coin;
using org::minima::objects::Token;
using org::minima::objects::Transaction;
using org::minima::objects::TxPoW;
using org::minima::objects::Witness;

using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;

using org::minima::system::Main;
using org::minima::system::brains::TxPoWGenerator;
using org::minima::system::brains::TxPoWMiner;
using org::minima::system::brains::TxPoWSearcher;

using org::minima::system::commands::backup::vault;
using org::minima::system::commands::search::keys;
using org::minima::system::commands::txn::txnutils;

using org::minima::system::params::GeneralParams;
using org::minima::system::params::GlobalParams;

using org::minima::utils::MinimaLogger;
using org::minima::utils::json::JSONArray;
using org::minima::utils::json::JSONObject;

// ----------------- send::AddressAmount -----------------
org::minima::system::commands::send::send::AddressAmount::AddressAmount(
    const MiniData& zAddress,
    const MiniNumber& zAmount)
    : mAddress(zAddress), mAmount(zAmount) {}

const MiniData& org::minima::system::commands::send::send::AddressAmount::getAddress() const { return mAddress; }
const MiniNumber& org::minima::system::commands::send::send::AddressAmount::getAmount() const { return mAmount; }

// ----------------- send -----------------
org::minima::system::commands::send::send::send()
    : Command("send", "(address:Mx..|0x..) (amount:) (multi:[address:amount,..]) (tokenid:) (state:{}) (password:) (burn:) (split:) (coinage:) (mine:) (debug:) (dryrun:) - Send Minima or Tokens to an address") {}

std::string org::minima::system::commands::send::send::getFullHelp() const {
    return std::string("\nsend\n"
        "\n"
        "Send Minima or custom tokens to a wallet or custom script address.\n"
        "\n"
        "Optionally, send to multiple addresses in one transaction; split UTxOs; add state variables or include a burn.\n"
        "\n"
        "address: (optional)\n"
        "    A Minima 0x or Mx wallet address or custom script address. Must also specify amount.\n"
        "\n"
        "amount: (optional)\n"
        "    The amount of Minima or custom tokens to send to the specified address.\n"
        "\n"
        "multi: (optional)\n"
        "    JSON Array listing addresses and amounts to send in one transaction.\n"
        "    Takes the format [address:amount,address2:amount2,..], with each set in double quotes.\n"
        "\n"
        "tokenid: (optional)\n"
        "    If sending a custom token, you must specify its tokenid. Defaults to Minima (0x00).\n"
        "\n"
        "state: (optional)\n"
        "    List of state variables, if sending coins to a script. A JSON object in the format {\"port\":\"value\",..}\n"
        "\n"
        "burn: (optional)\n"
        "    The amount of Minima to burn with this transaction.\n"
        "\n"
        "password: (optional)\n"
        "    If your Wallet is password locked you can unlock it for this one transaction - then relock it.\n"
        "\n"
        "split: (optional)\n"
        "    You can set the number of coins the recipient will receive, between 1 and 20. Default is 1.\n"
        "    The amount being sent will be split into multiple coins of equal value.\n"
        "    You can split your own coins by sending to your own address.\n"
        "    Useful if you want to send multiple transactions without waiting for change to be confirmed.\n"
        "\n"
        "coinage: (optional)\n"
        "    How old must the coins be in blocks.\n"
        "\n"
        "debug: (optional)\n"
        "    true or false, true will print more detailed logs.\n"
        "\n"
        "dryrun: (optional)\n"
        "    true or false, true will simulate the send transaction but not execute it.\n"
        "\n"
        "mine: (optional)\n"
        "    true or false - should you mine the transaction immediately.\n"
        "\n"
        "fromaddress: (optional)\n"
        "    Only use this address for input coins.\n"
        "\n"
        "signkey: (optional)\n"
        "    Sign the txn with only this key (use with fromaddress).\n"
        "\n"
        "storestate: (optional)\n"
        "    true or false - defaults to true. Should the output coins store the state (will still appear in NOTIFYCOIN messages).\n"
        "\n"
        "Examples:\n"
        "\n"
        "send address:0xFF.. amount:10\n"
        "\n"
        "send address:0xFF.. amount:10 tokenid:0xFED5.. burn:0.1\n"
        "\n"
        "send address:0xFF.. amount:10 split:5 burn:0.1\n"
        "\n"
        "send multi:[\"0xFF..:10\",\"0xEE..:10\",\"0xDD..:10\"] split:20\n"
        "\n"
        "send amount:1 address:0xFF.. state:{\"0\":\"0xEE..\",\"1\":\"0xDD..\"}\n");
}

std::vector<std::string> org::minima::system::commands::send::send::getValidParams() const {
    return std::vector<std::string>{
        "action","uid","address","amount","multi","tokenid","state","burn","coinage",
        "split","debug","dryrun","mine","password","storestate","fromaddress","signkey"
    };
}

// Minimal parser for state param: expects {"port":"value",...}, double-quoted strings only
static std::vector<std::pair<int,std::string>> parseSimpleStateObject(const std::string& json) {
    std::vector<std::pair<int,std::string>> out;
    auto ltrim = [](const std::string& s)->std::string {
        std::size_t i=0; while(i<s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i; return s.substr(i);
    };
    auto rtrim = [](const std::string& s)->std::string {
        if(s.empty()) {
            return s;
        }
        std::size_t i=s.size(); 
        while(i>0 && std::isspace(static_cast<unsigned char>(s[i-1]))) --i; 
        return s.substr(0,i);
    };
    auto trim = [&](const std::string& s)->std::string { return rtrim(ltrim(s)); };
    std::string s = trim(json);
    if(s.empty() || s=="{}") return out;
    if(s.front()!='{' || s.back()!='}') return out;
    s = s.substr(1, s.size()-2);
    std::vector<std::string> pairs;
    std::string cur;
    bool inq=false; char prev=0;
    for(char c : s){
        if(c=='\"' && prev!='\\') inq = !inq;
        if(c==',' && !inq){
            pairs.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
        prev=c;
    }
    if(!cur.empty()) pairs.push_back(cur);

    for(auto& p : pairs){
        auto kv = trim(p);
        std::size_t pos = std::string::npos;
        prev=0; inq=false;
        for(std::size_t i=0;i<kv.size();++i){
            char c=kv[i];
            if(c=='\"' && prev!='\\') inq = !inq;
            if(c==':' && !inq){ pos=i; break; }
            prev=c;
        }
        if(pos==std::string::npos) continue;
        std::string k = trim(kv.substr(0,pos));
        std::string v = trim(kv.substr(pos+1));
        if(k.size()>=2 && k.front()=='\"' && k.back()=='\"') k = k.substr(1,k.size()-2);
        if(v.size()>=2 && v.front()=='\"' && v.back()=='\"') v = v.substr(1,v.size()-2);
        try {
            int port = std::stoi(k);
            out.emplace_back(port, v);
        } catch(...) {
            // skip invalid
        }
    }
    return out;
}

std::unique_ptr<JSONObject> org::minima::system::commands::send::send::runCommand() {
    // This 'ret' object holds the shallow copy. We MUST NOT let it be
    // destroyed by an un-caught exception.
    auto ret = getJSONReply();

    // *** ADD THIS TRY BLOCK ***
    try {

        // Recipients
        std::vector<AddressAmount> recipients;
        // Total amount
        MiniNumber totalamount = MiniNumber::ZERO();

        // Multi send?
        if (existsParam("multi")) {
            std::unique_ptr<JSONArray> allrecips = getJSONArrayParam("multi");
            for (const auto& anyv : allrecips->elements()) {
                if (!anyv.has_value()) continue;
                const std::string* sendto = std::any_cast<std::string>(&anyv);
                if (!sendto) continue;

                std::size_t pos = sendto->find(':');
                if (pos == std::string::npos) {
                    throw org::minima::system::commands::CommandException("Invalid multi entry (missing ':'): " + *sendto);
                }
                std::string address = sendto->substr(0, pos);
                std::string amountstr = sendto->substr(pos + 1);

                MiniData addr;
                std::string lower = address;
                std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c){return std::tolower(c);});
                if (lower.find("mx", 0) == 0) {
                    try {
                        addr = Address::convertMinimaAddress(address);
                    } catch (const std::exception& exc) {
                        throw org::minima::system::commands::CommandException(exc.what());
                    }
                } else {
                    addr = MiniData(address);
                }

                MiniNumber amount(amountstr);
                totalamount = totalamount.add(amount);

                recipients.emplace_back(addr, amount);
            }
        } else {
            // Single recipient
            // MinimaLogger::log("DEBUG: Entered SINGLE address block."); // <-- DEBUG 1
            
            // Use getDataParam, which correctly converts Mx/0x to a MiniData object
            std::unique_ptr<MiniData> sendaddress_up = getDataParam("address");
            MiniData sendaddress = *sendaddress_up; // Copy the object from the unique_ptr
            
            // MinimaLogger::log(std::string("DEBUG: sendaddress created: ") + sendaddress.to0xString()); // <-- DEBUG 6

            // Check for the "ammount" typo
            std::unique_ptr<MiniNumber> sendamount_up;
            if(existsParam("amount")) {
                sendamount_up = getNumberParam("amount");
            } else {
                throw org::minima::system::commands::CommandException("Missing 'amount' parameter");
            }
            
            MiniNumber sendamount = *sendamount_up;
            totalamount = sendamount;

            recipients.emplace_back(sendaddress, sendamount);
            
            // MinimaLogger::log("DEBUG: sendaddress emplaced into recipients."); // <-- DEBUG 7
        }

        // Token ID
        std::string tokenid = getParam("tokenid", "0x00");

        // Debug / Dryrun
        bool debug = getBooleanParam("debug", false);
        bool dryrun = getBooleanParam("dryrun", false);
        if (dryrun) debug = true;

        // Store state in outputs
        bool storestate = getBooleanParam("storestate", true);

        // Burn amount (must be >= 0)
        std::unique_ptr<MiniNumber> burn_up = getNumberParam("burn", MiniNumber::ZERO());
        MiniNumber burn = *burn_up;
        if (burn.isLess(MiniNumber::ZERO())) {
            throw org::minima::system::commands::CommandException("Cannot have negative burn " + burn.toString());
        }

        // Split
        std::unique_ptr<MiniNumber> split_up = getNumberParam("split", MiniNumber::ONE());
        MiniNumber split = split_up->floor();
        if (split.isLess(MiniNumber::ONE()) || split.isMore(GeneralParams::MAX_SPLIT_COINS)) {
            throw org::minima::system::commands::CommandException(
                std::string("Split must be whole number from 1 to ") + GeneralParams::MAX_SPLIT_COINS.toString());
        }

        // Burn included for Minima token only
        if (tokenid == "0x00") {
            totalamount = totalamount.add(burn);
        }

        // State JSON (parsed as string for robustness)
        std::vector<std::pair<int,std::string>> statepairs;
        if (existsParam("state")) {
            std::string state_str = getParam("state", "{}");
            statepairs = parseSimpleStateObject(state_str);
        }

        // Mining sync?
        bool minesync = getBooleanParam("mine", false);

        // DBs and miner
        TxPoWDB& txpdb = MinimaDB::getDB()->getTxPoWDB();
        TxPoWMiner* txminer = &Main::getInstance()->getTxPoWMiner();

        // Tip of tree
        auto tip = MinimaDB::getDB()->getTxPoWTree().getTip();
        if (!tip) {
             throw org::minima::system::commands::CommandException("No network tip available. Node still starting?");
        }

        // Coinage
        std::unique_ptr<MiniNumber> coinage_up = getNumberParam("coinage", GlobalParams::MINIMA_CONFIRM_DEPTH);
        MiniNumber coinage = *coinage_up;
        if (coinage.isLess(GlobalParams::MINIMA_CONFIRM_DEPTH)) {
            throw org::minima::system::commands::CommandException(
                "Coinage MUST be >= " + GlobalParams::MINIMA_CONFIRM_DEPTH.toString());
        }

        // From address filter and signkey
        std::string usepubkey = getAddressParam("signkey", "");
        bool usefromaddress = false;
        
        // Get the fromaddress as MiniData directly, using an empty MiniData as the default
        std::unique_ptr<MiniData> fromaddress_up = getDataParam("fromaddress", MiniData());
        MiniData fromaddress_data = *fromaddress_up; // Copy from the unique_ptr

        if (fromaddress_data.getLength() > 0) {
            if (debug) {
                MinimaLogger::log(std::string("Search only coins with address : ") + fromaddress_data.to0xString());
            }
            usefromaddress = true;
        }

        // Build transaction input coin list
        std::vector<std::shared_ptr<Coin>> foundcoins;
        if (usefromaddress) {
            // Only search a specific address
            foundcoins = TxPoWSearcher::searchCoins(
                tip, true,
                false, MiniData::ZERO_TXPOWID(),
                false, MiniNumber::ZERO(),
                true, fromaddress_data, // Use the MiniData object directly
                true, MiniData(tokenid),
                false);
        } else {
            foundcoins = TxPoWSearcher::getRelevantUnspentCoins(tip, tokenid, true);
        }

        std::vector<std::shared_ptr<Coin>> relcoins;

        // Now ensure coins are old enough
        MiniNumber mincoinblock = tip->getBlockNumber().sub(coinage);
        for (auto& relc : foundcoins) {
            if (relc->getBlockCreated().isLessEqual(mincoinblock)) {
                relcoins.push_back(relc);
            }
        }

        // *** THIS IS THE EXCEPTION THAT IS BEING THROWN ***
        if (relcoins.empty()) {
            throw org::minima::system::commands::CommandException("No Coins of tokenid:" + tokenid + " available!");
        }

        // Select correct coins
        MiniNumber findamount = totalamount;
        if (tokenid != "0x00") {
            Token* ftok = relcoins[0]->getToken();
            if (!ftok) {
                throw org::minima::system::commands::CommandException("Token information missing for selected coin");
            }
            auto req_minima_up = ftok->getScaledMinimaAmount(totalamount);
            findamount = *req_minima_up;
        }

        relcoins = selectCoins(relcoins, findamount, debug);

        // Add coins until we reach totalamount (in the correct unit)
        MiniNumber currentamount = MiniNumber::ZERO();
        std::vector<std::shared_ptr<Coin>> currentcoins;

        if (debug) {
            MinimaLogger::log("Coins that will be checked for transaction");
            for (auto& coin : relcoins) {
                MinimaLogger::log("Coin : " + coin->getAmount().toString() + " " + coin->getCoinID().to0xString());
            }
        }

        Token* token = nullptr;
        for (auto& coin : relcoins) {
            // Already being mined?
            if (txminer->checkForMiningCoin(coin->getCoinID().to0xString())) {
                if (debug) {
                    MinimaLogger::log("Coin being mined : " + coin->getAmount().toString() + " " + coin->getCoinID().to0xString());
                }
                continue;
            }
            // In mempool?
            if (txpdb.checkMempoolCoins(coin->getCoinID())) {
                if (debug) {
                    MinimaLogger::log("Coin in mempool : " + coin->getAmount().toString() + " " + coin->getCoinID().to0xString());
                }
                continue;
            }

            currentcoins.push_back(coin);

            if (tokenid == "0x00") {
                currentamount = currentamount.add(coin->getAmount());
            } else {
                if (token == nullptr) token = coin->getToken();
                auto amt_up = coin->getToken()->getScaledTokenAmount(coin->getAmount());
                currentamount = currentamount.add(*amt_up);
            }

            if (debug) {
                MinimaLogger::log("Coin added : " + coin->getAmount().toString() + " " + coin->getCoinID().to0xString()
                                  + " total:" + currentamount.toString());
            }

            if (currentamount.isMoreEqual(totalamount)) {
                break;
            }
        }

        // Check token script
        if (token != nullptr) {
            std::string script = token->getTokenScript().toString();
            if (script != "RETURN TRUE") {
                ret->put("status", false);
                ret->put("message", std::string("Token script is not simple : ") + script);
                return ret;
            }
        }

        // Enough funds?
        if (currentamount.isLess(totalamount)) {
            ret->put("status", false);
            ret->put("message", std::string("Insufficient funds.. you only have ") + currentamount.toString()
                                   + " require:" + totalamount.toString());
            return ret;
        }

        if (debug) {
            MinimaLogger::log("Total Coins used : " + std::to_string(currentcoins.size()));
        }

        // Change
        MiniNumber change = currentamount.sub(totalamount);

        // Build transaction
        Transaction transaction;
        Witness witness;

        // Add inputs (deep copy)
        // for (auto& incoin : currentcoins) {
        //     std::unique_ptr<Coin> incp = incoin->deepCopy();
        //     transaction.addInput(std::move(incp));
        //     if (debug) {
        //         MinimaLogger::log(std::string("Input : ") + transaction.getAllInputs().back()->toJSON().toString());
        //     }
        // }

        Wallet& walletdb = MinimaDB::getDB()->getWallet();
        std::vector<std::string> reqsigs;
        std::vector<std::string> addedcoinid;

        // Get the MMR Node to build proofs
        MiniNumber minblock = MiniNumber::ZERO();
        for(auto& inputs : currentcoins) {
            if(inputs->getBlockCreated().isMore(minblock)) {
                minblock = inputs->getBlockCreated();
            }
        }
        MiniNumber currentblock = tip->getBlockNumber();
        MiniNumber blockdiff 	= currentblock.sub(minblock);
        if(blockdiff.isMore(GlobalParams::MINIMA_MMR_PROOF_HISTORY)) {
            blockdiff = GlobalParams::MINIMA_MMR_PROOF_HISTORY;
        }
        std::shared_ptr<TxPoWTreeNode> mmrnode = tip->getPastNode(tip->getBlockNumber().sub(blockdiff));
        if(mmrnode == nullptr) {
            throw org::minima::system::commands::CommandException("Not enough blocks in chain to make valid MMR Proofs..");
        }

        // Collect coin IDs and required public keys (for signing)
        for (auto& input : currentcoins) {
            
            // 1. Add the input to our transaction
            // (Java adds the original object, not a deep copy, but copy is safer in C++)
            // std::unique_ptr<Coin> incp = input->deepCopy();
            // transaction.addInput(std::move(incp));

            transaction.addInput(input);
            
            if (debug) {
                MinimaLogger::log(std::string("Input : ") + input->toJSON().toString());
            }

            // Add to coinid list (for burn checks)
            addedcoinid.push_back(input->getCoinID().to0xString());

            // 2. Create and add the MMRProof for the coin
            org::minima::objects::mmr::MMRProof proof_obj = 
                mmrnode->getMMR().getProofToPeak(input->getMMREntryNumber());
            std::shared_ptr<org::minima::objects::mmr::MMRProof> proof_ptr = 
                std::make_shared<org::minima::objects::mmr::MMRProof>(proof_obj);
            auto cp = std::make_unique<org::minima::objects::CoinProof>(input, proof_ptr);
            witness.addCoinProof(std::move(cp));
            
            // 3. Get the ScriptRow and add the ScriptProof
            std::string scraddress 	= input->getAddress().to0xString();
            std::unique_ptr<ScriptRow> srow = walletdb.getScriptFromAddress(scraddress);
            if (!srow) {
                throw org::minima::system::commands::CommandException(
                    std::string("SERIOUS ERROR script missing for simple address : ") + scraddress);
            }
            
            auto pscr = std::make_unique<org::minima::objects::ScriptProof>(srow->getScript());
            witness.addScript(std::move(pscr));

            // 4. Get the PublicKey from that ScriptRow and add it to reqsigs
            std::string pubkey = srow->getPublicKey();
            if (std::find(reqsigs.begin(), reqsigs.end(), pubkey) == reqsigs.end()) {
                reqsigs.push_back(pubkey);
            }
        }

        // Specific sign key?
        if (!usepubkey.empty()) {
            reqsigs.clear();
            reqsigs.push_back(usepubkey);
        }

        // Validate amounts for token/minima
        if (tokenid != "0x00") {
            if (token == nullptr) {
                throw org::minima::system::commands::CommandException("Token not available for scaling");
            }
            std::unique_ptr<MiniNumber> tokenamount_up = token->getScaledMinimaAmount(totalamount);
            MiniNumber tokenamount = *tokenamount_up;
            std::unique_ptr<MiniNumber> prectest_up = token->getScaledTokenAmount(tokenamount);
            MiniNumber prectest = *prectest_up;

            if (!prectest.isEqual(totalamount)) {
                throw org::minima::system::commands::CommandException(
                    std::string("Invalid Token amount to send.. ") + totalamount.toString());
            }
            totalamount = tokenamount;
        } else {
            if (!totalamount.isValidMinimaValue()) {
                throw org::minima::system::commands::CommandException(
                    std::string("Invalid Minima amount to send.. ") + totalamount.toString());
            }
        }

        int isplit = split.getAsInt();

        // Outputs for each recipient
        for (const auto& user : recipients) {
            MiniData address = user.getAddress();

            MiniNumber usertotal = user.getAmount();
            if (tokenid != "0x00") {
                std::unique_ptr<MiniNumber> scaled_up = token->getScaledMinimaAmount(usertotal);
                usertotal = *scaled_up;
            }
            MiniNumber currenttotal = MiniNumber::ZERO();

            MiniNumber splitamount = usertotal.div(split);
            if (splitamount.isLessEqual(MiniNumber::ZERO())) {
                throw org::minima::system::commands::CommandException(
                    std::string("Cannot have ZERO output - output is too small for this user.. ") + user.getAddress().to0xString());
            }

            for (int i = 0; i < isplit; ++i) {
                auto recipient = std::make_unique<Coin>(Coin::COINID_OUTPUT, address, splitamount, Token::TOKENID_MINIMA, storestate);
                currenttotal = currenttotal.add(splitamount);

                if (tokenid != "0x00") {
                    recipient->resetTokenID(MiniData(tokenid));
                    std::unique_ptr<MiniData> tokmd = MiniData::getMiniDataVersion(*token);
                    std::unique_ptr<Token> tokcp = Token::convertMiniDataVersion(*tokmd);
                    recipient->setToken(std::move(tokcp));
                }

                if (debug) {
                    MinimaLogger::log(std::string("Output : ") + recipient->toJSON().toString());
                }

                transaction.addOutput(std::move(recipient));
            }

            // Remainder for this recipient after equal split
            MiniNumber currentdiff = usertotal.sub(currenttotal);
            if (currentdiff.isMore(MiniNumber::ZERO())) {
                auto remaincoin = std::make_unique<Coin>(Coin::COINID_OUTPUT, address, currentdiff, Token::TOKENID_MINIMA, storestate);
                if (tokenid != "0x00") {
                    remaincoin->resetTokenID(MiniData(tokenid));
                    std::unique_ptr<MiniData> tokmd = MiniData::getMiniDataVersion(*token);
                    std::unique_ptr<Token> tokcp = Token::convertMiniDataVersion(*tokmd);
                    remaincoin->setToken(std::move(tokcp));
                }

                if (debug) {
                    MinimaLogger::log(std::string("Rounding Output from split : ") + remaincoin->toJSON().toString());
                }

                transaction.addOutput(std::move(remaincoin));
            }
        }

        // Change
        if (debug) {
            MinimaLogger::log(std::string("Change amount : ") + change.toString());
        }

        if (change.isMore(MiniNumber::ZERO())) {
            std::unique_ptr<ScriptRow> newwalletaddress = MinimaDB::getDB()->getWallet().getDefaultAddress();
            if (MinimaDB::getDB()->getWallet().isBaseSeedAvailable()) {
                if (!keys::checkKey(newwalletaddress->getPublicKey())) {
                    throw org::minima::system::commands::CommandException(
                        std::string("[!] SERIOUS ERROR - INCORRECT Public key : ") + newwalletaddress->getPublicKey());
                }
            }

            MiniData chgaddress(newwalletaddress->getAddress());
            if (usefromaddress) {
                chgaddress = fromaddress_data; // Use the MiniData object directly
            }

            MiniNumber changeamount = change;
            if (tokenid != "0x00") {
                changeamount = *token->getScaledMinimaAmount(change);
            }

            auto changecoin = std::make_unique<Coin>(Coin::COINID_OUTPUT, chgaddress, changeamount, Token::TOKENID_MINIMA, false);
            if (tokenid != "0x00") {
                changecoin->resetTokenID(MiniData(tokenid));
                std::unique_ptr<MiniData> tokmd = MiniData::getMiniDataVersion(*token);
                std::unique_ptr<Token> tokcp = Token::convertMiniDataVersion(*tokmd);
                changecoin->setToken(std::move(tokcp));
            }

            if (debug) {
                MinimaLogger::log(std::string("Change Output : ") + changecoin->toJSON().toString());
            }

            transaction.addOutput(std::move(changecoin));
        }

        // Output count limit
        int outsize = static_cast<int>(transaction.getAllOutputs().size());
        if (outsize > GeneralParams::MAX_RELAY_OUTPUTCOINS) {
            throw org::minima::system::commands::CommandException(
                std::string("Too many outputs ") + std::to_string(outsize) + " - will not be relayed by network");
        }

        // State variables
        for (const auto& pr : statepairs) {
            int port = pr.first;
            const std::string& var = pr.second;
            auto sv = std::make_unique<org::minima::objects::StateVariable>(port, var);
            transaction.addStateVariable(std::move(sv));
        }

        // Set MMR and Script proofs for the whole transaction
        // txnutils::setMMRandScripts(transaction, witness);

        // Precompute CoinIDs for outputs and TXID
        TxPoWGenerator::precomputeTransactionCoinID(transaction);
        transaction.calculateTransactionID();

        if (debug) {
            if (!usepubkey.empty()) {
                MinimaLogger::log(std::string("(SIGNKEYS) Total signatures required : ") + std::to_string(reqsigs.size()));
            } else {
                MinimaLogger::log(std::string("Total signatures required : ") + std::to_string(reqsigs.size()));
            }
        }

        // Password unlock if requested and not dryrun
        bool passwordlock = false;
        if (!dryrun) {
            if (existsParam("password")) {
                if (MinimaDB::getDB()->getWallet().isBaseSeedAvailable()) {
                    throw org::minima::system::commands::CommandException("WalletDB NOT Locked! Password Invalid");
                }
                if (debug) {
                    MinimaLogger::log("Unlocking password DB");
                }
                vault::passowrdUnlockDB(getParam("password"));
                passwordlock = true;
            }
        } else {
            MinimaLogger::log("DRYRUN so NOT Unlocking password DB");
        }

        // Sign
        for (const std::string& pubkey : reqsigs) {
            MiniData txid = transaction.getTransactionID();
            // MinimaLogger::log("DEBUG_SIGN: About to sign with pubkey: " + pubkey);
            // MinimaLogger::log("DEBUG_SIGN: TxID to sign: " + txid.to0xString());
            // MinimaLogger::log("DEBUG_SIGN: TxID length: " + std::to_string(txid.getLength()));
            
            if (!dryrun) {
                try {
                    std::unique_ptr<org::minima::objects::keys::Signature> signature =
                        MinimaDB::getDB()->getWallet().signData(pubkey, txid);
                    
                    // MinimaLogger::log("DEBUG_SIGN: Signature created, root pubkey: " + 
                    //                 signature->getRootPublicKey().to0xString());
                    witness.addSignature(std::move(signature));
                    
                    // MinimaLogger::log("DEBUG_WITNESS: Added signature to witness");
                    // MinimaLogger::log("DEBUG_WITNESS: Total signatures in witness: " + 
                    //     std::to_string(witness.getAllSignatures().size()));

                    // auto allKeys = witness.getAllSignatureKeys();
                    // MinimaLogger::log("DEBUG_WITNESS: Signature keys:");
                    // for (const auto& key : allKeys) {
                    //     MinimaLogger::log("  - " + key.to0xString());
                    // }

                    
                    // std::string testKey = "0xB94CFEB80BC2E476BEAB70C222FDD5AE842F26C32E37428B99F1C7E00E2D2D83";
                    // bool isSigned = witness.isSignedBy(testKey);
                    // MinimaLogger::log("DEBUG_WITNESS: isSignedBy(0xB94CF...) = " + std::string(isSigned ? "TRUE" : "FALSE"));
                    

                } catch (const std::exception& e) {
                    // MinimaLogger::log("DEBUG_SIGN: Signature creation failed: " + std::string(e.what()));
                    throw org::minima::system::commands::CommandException(e.what());
                }
            }
        }

        // Final TxPoW
        std::unique_ptr<org::minima::objects::TxPoW> txpow_ptr;

        // Burn transaction?
        if (tokenid != "0x00" && burn.isMore(MiniNumber::ZERO())) {
            org::minima::database::userprefs::txndb::TxnRow burntxn = txnutils::createBurnTransaction(addedcoinid, transaction.getTransactionID(), burn);
            txpow_ptr = TxPoWGenerator::generateTxPoW(transaction, witness, &burntxn.getTransaction(), &burntxn.getWitness());
        } else {
            txpow_ptr = TxPoWGenerator::generateTxPoW(transaction, witness);
        }

        txpow_ptr->calculateTXPOWID();

        long long size = txpow_ptr->getSizeinBytesWithoutBlockTxns();
        long long max = tip->getTxPoW().getMagic().getMaxTxPoWSize().getAsLong();
        if (debug) {
            MinimaLogger::log(std::string("TxPoW size ") + std::to_string(size) + " max:" + std::to_string(max));
        }

        if (size > max) {
            if (!dryrun && passwordlock) {
                vault::passwordLockDB(getParam("password"));
            }
            throw org::minima::system::commands::CommandException(
                std::string("TxPoW size too large.. ") + std::to_string(size) + "/" + std::to_string(max));
        }

        if (!dryrun) {
            if (passwordlock) {
                vault::passwordLockDB(getParam("password"));
            }
        }

        if (!dryrun) {
            if (minesync) {
                bool success = Main::getInstance()->getTxPoWMiner().MineMaxTxPoW(*txpow_ptr, 120000);
                if (!success) {
                    throw org::minima::system::commands::CommandException("FAILED TO MINE txn in 120 seconds !?");
                }
            } else {
                Main::getInstance()->getTxPoWMiner().mineTxPoWAsync(*txpow_ptr);
            }
        } else {
            MinimaLogger::log("DRY RUN - not sending");
        }

        ret->put("dryrun", dryrun);
        ret->put("response", txpow_ptr->toJSON());

        if (dryrun) {
            JSONObject sizes;
            sizes.put("txpow", static_cast<std::int64_t>(txpow_ptr->getSizeinBytes()));

            JSONObject inputcoins;
            for (const auto& up : txpow_ptr->getTransaction().getAllInputs()) {
                const Coin* cc = up.get();
                std::unique_ptr<MiniData> cd = MiniData::getMiniDataVersion(const_cast<Coin&>(*cc));
                inputcoins.put(cc->getCoinID().to0xString(), cd->getLength());
            }

            JSONObject outputcoins;
            for (const auto& up : txpow_ptr->getTransaction().getAllOutputs()) {
                const Coin* cc = up.get();
                std::unique_ptr<MiniData> cd = MiniData::getMiniDataVersion(const_cast<Coin&>(*cc));
                outputcoins.put(cc->getCoinID().to0xString(), cd->getLength());
            }

            sizes.put("inputcoins", inputcoins);
            sizes.put("outputcoins", outputcoins);

            std::unique_ptr<MiniData> wd = MiniData::getMiniDataVersion(txpow_ptr->getWitness());
            sizes.put("witness", wd->getLength());

            ret->put("bytesize", sizes);
        }

        // This is the normal, successful return
        return ret;

    // *** ADD THESE CATCH BLOCKS at the end of the function ***
    } catch (const org::minima::system::commands::CommandException& exc) {
        // We caught the exception. 'ret' is NOT destroyed.
        // We just add the error to 'ret' and return it.
        ret->put("status", false);
        ret->put("error", std::string(exc.what()));
        return ret;
    } catch (const std::exception& exc) {
        // Catch any other standard exceptions too
        ret->put("status", false);
        ret->put("error", std::string(exc.what()));
        return ret;
    }
}

org::minima::system::commands::Command* org::minima::system::commands::send::send::getFunction() {
    return new send();
}

// ----------------- Static Coin selection helpers -----------------
std::vector<std::shared_ptr<Coin>>
org::minima::system::commands::send::send::selectCoins(
    const std::vector<std::shared_ptr<Coin>>& zAllCoins,
    const MiniNumber& zAmountRequired) {
    return selectCoins(zAllCoins, zAmountRequired, false);
}

std::vector<std::shared_ptr<Coin>>
org::minima::system::commands::send::send::selectCoins(
    const std::vector<std::shared_ptr<Coin>>& zAllCoins,
    const MiniNumber& zAmountRequired,
    bool zDebug) {

    std::vector<std::shared_ptr<Coin>> ret;

    TxPoWDB& txpdb = MinimaDB::getDB()->getTxPoWDB();
    TxPoWMiner* txminer = &Main::getInstance()->getTxPoWMiner();

    // Sort by amount and address
    std::vector<std::shared_ptr<Coin>> coinlist = orderCoins(zAllCoins);

    if (zDebug) {
        MinimaLogger::log("All Selection coins");
        for (auto& coin : coinlist) {
            MinimaLogger::log("Coin found : " + coin->getAmount().toString()
                              + " " + coin->getCoinID().to0xString()
                              + " @ " + coin->getAddress().to0xString());
        }
        MinimaLogger::log("Now checking coins");
    }

    bool found = false;
    std::shared_ptr<Coin> currentcoin;
    for (auto& coin : coinlist) {

        if (txminer->checkForMiningCoin(coin->getCoinID().to0xString())) {
            if (zDebug) {
                MinimaLogger::log("Coin being mined : " + coin->getAmount().toString()
                                  + " " + coin->getCoinID().to0xString());
            }
            continue;
        }

        if (txpdb.checkMempoolCoins(coin->getCoinID())) {
            if (zDebug) {
                MinimaLogger::log("Coin in mempool : " + coin->getAmount().toString()
                                  + " " + coin->getCoinID().to0xString());
            }
            continue;
        }

        if (coin->getAmount().isMoreEqual(zAmountRequired)) {
            if (zDebug) {
                MinimaLogger::log("Valid Coin found : " + coin->getAmount().toString()
                                  + " " + coin->getCoinID().to0xString());
            }
            found = true;
            currentcoin = coin;
        } else {
            if (zDebug) {
                MinimaLogger::log("Coin too small - no more checking : " + coin->getAmount().toString()
                                  + " " + coin->getCoinID().to0xString());
            }
            break;
        }
    }

    if (found) {
        ret.push_back(currentcoin);
        if (zDebug) {
            MinimaLogger::log("Single coin returned : " + currentcoin->getAmount().toString()
                              + " " + currentcoin->getCoinID().to0xString());
        }
        return ret;
    }

    if (zDebug) {
        MinimaLogger::log("Returning all coins..");
    }
    return coinlist;
}

std::vector<std::shared_ptr<Coin>>
org::minima::system::commands::send::send::orderCoins(
    const std::vector<std::shared_ptr<Coin>>& zCoins) {

    std::vector<std::shared_ptr<Coin>> sorted = zCoins;

    // First sort by amount descending
    std::sort(sorted.begin(), sorted.end(),
        [](const std::shared_ptr<Coin>& a, const std::shared_ptr<Coin>& b){
            const MiniNumber& amt1 = a->getAmount();
            const MiniNumber& amt2 = b->getAmount();
            return amt2.isLess(amt1); // desc: true if a > b
        });

    // Collect distinct addresses in order of first appearance
    std::vector<std::string> addresses;
    for (const auto& cc : sorted) {
        std::string addr = cc->getAddress().to0xString();
        if (std::find(addresses.begin(), addresses.end(), addr) == addresses.end()) {
            addresses.push_back(addr);
        }
    }

    // Group by address
    std::vector<std::shared_ptr<Coin>> ret;
    for (const std::string& address : addresses) {
        for (const auto& cc : sorted) {
            if (cc->getAddress().to0xString() == address) {
                ret.push_back(cc);
            }
        }
    }

    return ret;
}