// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Crowncoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"
#include "core.h"
#include "db.h"
#include "init.h"
#include "activethrone.h"
#include "throneman.h"
#include "throneconfig.h"
#include "rpcserver.h"
#include <boost/lexical_cast.hpp>

#include <fstream>
using namespace json_spirit;
using namespace std;

Value darksend(const Array& params, bool fHelp)
{
    if (fHelp || params.size() == 0)
        throw runtime_error(
            "darksend <dashaddress> <amount>\n"
            "dashaddress, reset, or auto (AutoDenominate)"
            "<amount> is a real and is rounded to the nearest 0.00000001"
            + HelpRequiringPassphrase());

    if (pwalletMain->IsLocked())
        throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED, "Error: Please enter the wallet passphrase with walletpassphrase first.");

    if(params[0].get_str() == "auto"){
        if(fThroNe)
            return "DarkSend is not supported from thrones";

        darkSendPool.DoAutomaticDenominating();
        return "DoAutomaticDenominating";
    }

    if(params[0].get_str() == "reset"){
        darkSendPool.SetNull(true);
        darkSendPool.UnlockCoins();
        return "successfully reset darksend";
    }

    if (params.size() != 2)
        throw runtime_error(
            "darksend <dashaddress> <amount>\n"
            "dashaddress, denominate, or auto (AutoDenominate)"
            "<amount> is a real and is rounded to the nearest 0.00000001"
            + HelpRequiringPassphrase());

    CCrowncoinAddress address(params[0].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Dash address");

    // Amount
    int64_t nAmount = AmountFromValue(params[1]);

    // Wallet comments
    CWalletTx wtx;
    string strError = pwalletMain->SendMoneyToDestination(address.Get(), nAmount, wtx, ONLY_DENOMINATED);
    if (strError != "")
        throw JSONRPCError(RPC_WALLET_ERROR, strError);

    return wtx.GetHash().GetHex();
}


Value getpoolinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getpoolinfo\n"
            "Returns an object containing anonymous pool-related information.");

    Object obj;
    obj.push_back(Pair("current_throne",        mnodeman.GetCurrentThroNe()->addr.ToString()));
    obj.push_back(Pair("state",        darkSendPool.GetState()));
    obj.push_back(Pair("entries",      darkSendPool.GetEntriesCount()));
    obj.push_back(Pair("entries_accepted",      darkSendPool.GetCountEntriesAccepted()));
    return obj;
}


Value throne(const Array& params, bool fHelp)
{
    string strCommand;
    if (params.size() >= 1)
        strCommand = params[0].get_str();

    if (fHelp  ||
        (strCommand != "start" && strCommand != "start-alias" && strCommand != "start-many" && strCommand != "stop" && strCommand != "stop-alias" && strCommand != "stop-many" && strCommand != "list" && strCommand != "list-conf" && strCommand != "count"  && strCommand != "enforce"
            && strCommand != "debug" && strCommand != "current" && strCommand != "winners" && strCommand != "genkey" && strCommand != "connect" && strCommand != "outputs" && strCommand != "vote-many" && strCommand != "vote"))
        throw runtime_error(
                "throne \"command\"... ( \"passphrase\" )\n"
                "Set of commands to execute throne related actions\n"
                "\nArguments:\n"
                "1. \"command\"        (string or set of strings, required) The command to execute\n"
                "2. \"passphrase\"     (string, optional) The wallet passphrase\n"
                "\nAvailable commands:\n"
                "  count        - Print number of all known thrones (optional: 'enabled', 'both')\n"
                "  current      - Print info on current throne winner\n"
                "  debug        - Print throne status\n"
                "  genkey       - Generate new throneprivkey\n"
                "  enforce      - Enforce throne payments\n"
                "  outputs      - Print throne compatible outputs\n"
                "  start        - Start throne configured in dash.conf\n"
                "  start-alias  - Start single throne by assigned alias configured in throne.conf\n"
                "  start-many   - Start all thrones configured in throne.conf\n"
                "  stop         - Stop throne configured in dash.conf\n"
                "  stop-alias   - Stop single throne by assigned alias configured in throne.conf\n"
                "  stop-many    - Stop all thrones configured in throne.conf\n"
                "  list         - Print list of all known thrones (see thronelist for more info)\n"
                "  list-conf    - Print throne.conf in JSON format\n"
                "  winners      - Print list of throne winners\n"
                "  vote-many    - Vote on a Dash initiative\n"
                "  vote         - Vote on a Dash initiative\n"
                );


    if (strCommand == "stop")
    {
        if(!fThroNe) return "you must set throne=1 in the configuration";

        if(pwalletMain->IsLocked()) {
            SecureString strWalletPass;
            strWalletPass.reserve(100);

            if (params.size() == 2){
                strWalletPass = params[1].get_str().c_str();
            } else {
                throw runtime_error(
                    "Your wallet is locked, passphrase is required\n");
            }

            if(!pwalletMain->Unlock(strWalletPass)){
                return "incorrect passphrase";
            }
        }

        std::string errorMessage;
        if(!activeThrone.StopThroNe(errorMessage)) {
        	return "stop failed: " + errorMessage;
        }
        pwalletMain->Lock();

        if(activeThrone.status == THRONE_STOPPED) return "successfully stopped throne";
        if(activeThrone.status == THRONE_NOT_CAPABLE) return "not capable throne";

        return "unknown";
    }

    if (strCommand == "stop-alias")
    {
	    if (params.size() < 2){
			throw runtime_error(
			"command needs at least 2 parameters\n");
	    }

	    std::string alias = params[1].get_str().c_str();

    	if(pwalletMain->IsLocked()) {
    		SecureString strWalletPass;
    	    strWalletPass.reserve(100);

			if (params.size() == 3){
				strWalletPass = params[2].get_str().c_str();
			} else {
				throw runtime_error(
				"Your wallet is locked, passphrase is required\n");
			}

			if(!pwalletMain->Unlock(strWalletPass)){
				return "incorrect passphrase";
			}
        }

    	bool found = false;

		Object statusObj;
		statusObj.push_back(Pair("alias", alias));

    	BOOST_FOREACH(CThroneConfig::CThroneEntry mne, throneConfig.getEntries()) {
    		if(mne.getAlias() == alias) {
    			found = true;
    			std::string errorMessage;
    			bool result = activeThrone.StopThroNe(mne.getIp(), mne.getPrivKey(), errorMessage);

				statusObj.push_back(Pair("result", result ? "successful" : "failed"));
    			if(!result) {
   					statusObj.push_back(Pair("errorMessage", errorMessage));
   				}
    			break;
    		}
    	}

    	if(!found) {
    		statusObj.push_back(Pair("result", "failed"));
    		statusObj.push_back(Pair("errorMessage", "could not find alias in config. Verify with list-conf."));
    	}

    	pwalletMain->Lock();
    	return statusObj;
    }

    if (strCommand == "stop-many")
    {
    	if(pwalletMain->IsLocked()) {
			SecureString strWalletPass;
			strWalletPass.reserve(100);

			if (params.size() == 2){
				strWalletPass = params[1].get_str().c_str();
			} else {
				throw runtime_error(
				"Your wallet is locked, passphrase is required\n");
			}

			if(!pwalletMain->Unlock(strWalletPass)){
				return "incorrect passphrase";
			}
		}

		int total = 0;
		int successful = 0;
		int fail = 0;


		Object resultsObj;

		BOOST_FOREACH(CThroneConfig::CThroneEntry mne, throneConfig.getEntries()) {
			total++;

			std::string errorMessage;
			bool result = activeThrone.StopThroNe(mne.getIp(), mne.getPrivKey(), errorMessage);

			Object statusObj;
			statusObj.push_back(Pair("alias", mne.getAlias()));
			statusObj.push_back(Pair("result", result ? "successful" : "failed"));

			if(result) {
				successful++;
			} else {
				fail++;
				statusObj.push_back(Pair("errorMessage", errorMessage));
			}

			resultsObj.push_back(Pair("status", statusObj));
		}
		pwalletMain->Lock();

		Object returnObj;
		returnObj.push_back(Pair("overall", "Successfully stopped " + boost::lexical_cast<std::string>(successful) + " thrones, failed to stop " +
				boost::lexical_cast<std::string>(fail) + ", total " + boost::lexical_cast<std::string>(total)));
		returnObj.push_back(Pair("detail", resultsObj));

		return returnObj;

    }

    if (strCommand == "list")
    {
        Array newParams(params.size() - 1);
        std::copy(params.begin() + 1, params.end(), newParams.begin());
        return thronelist(newParams, fHelp);
    }

    if (strCommand == "count")
    {
        if (params.size() > 2){
            throw runtime_error(
            "too many parameters\n");
        }
        if (params.size() == 2)
        {
            if(params[1] == "enabled") return mnodeman.CountEnabled();
            if(params[1] == "both") return boost::lexical_cast<std::string>(mnodeman.CountEnabled()) + " / " + boost::lexical_cast<std::string>(mnodeman.size());
        }
        return mnodeman.size();
    }

    if (strCommand == "start")
    {
        if(!fThroNe) return "you must set throne=1 in the configuration";

        if(pwalletMain->IsLocked()) {
            SecureString strWalletPass;
            strWalletPass.reserve(100);

            if (params.size() == 2){
                strWalletPass = params[1].get_str().c_str();
            } else {
                throw runtime_error(
                    "Your wallet is locked, passphrase is required\n");
            }

            if(!pwalletMain->Unlock(strWalletPass)){
                return "incorrect passphrase";
            }
        }

        if(activeThrone.status != THRONE_REMOTELY_ENABLED && activeThrone.status != THRONE_IS_CAPABLE){
            activeThrone.status = THRONE_NOT_PROCESSED; // TODO: consider better way
            std::string errorMessage;
            activeThrone.ManageStatus();
            pwalletMain->Lock();
        }

        if(activeThrone.status == THRONE_REMOTELY_ENABLED) return "throne started remotely";
        if(activeThrone.status == THRONE_INPUT_TOO_NEW) return "throne input must have at least 15 confirmations";
        if(activeThrone.status == THRONE_STOPPED) return "throne is stopped";
        if(activeThrone.status == THRONE_IS_CAPABLE) return "successfully started throne";
        if(activeThrone.status == THRONE_NOT_CAPABLE) return "not capable throne: " + activeThrone.notCapableReason;
        if(activeThrone.status == THRONE_SYNC_IN_PROCESS) return "sync in process. Must wait until client is synced to start.";

        return "unknown";
    }

    if (strCommand == "start-alias")
    {
        if (params.size() < 2){
            throw runtime_error(
            "command needs at least 2 parameters\n");
        }

        std::string alias = params[1].get_str().c_str();

        if(pwalletMain->IsLocked()) {
            SecureString strWalletPass;
            strWalletPass.reserve(100);

            if (params.size() == 3){
                strWalletPass = params[2].get_str().c_str();
            } else {
                throw runtime_error(
                "Your wallet is locked, passphrase is required\n");
            }

            if(!pwalletMain->Unlock(strWalletPass)){
                return "incorrect passphrase";
            }
        }

        bool found = false;
        bool found2 = false;

        Object statusObj;
        statusObj.push_back(Pair("alias", alias));

        BOOST_FOREACH(CThroneConfig::CThroneEntry mne, throneConfig.getEntries()) {
            if(mne.getAlias() == alias) {
                found = true;
                std::string errorMessage;
                std::vector<CThrone> vThrones = mnodeman.GetFullThroneVector();
                BOOST_FOREACH(CThrone& mn, vThrones) {
                    std::string strAddr = mn.addr.ToString();
                    if (strAddr == mne.getIp().ToString()){
                        found2 = true;
                        found = false;
                    }
                }
                bool result;
                if (!found2){
                    result = activeThrone.Register(mne.getIp(), mne.getPrivKey(), mne.getTxHash(), mne.getOutputIndex(), errorMessage);
                } else {
                    errorMessage = "Throne has already been started and your IP added to the list.";
                }

                statusObj.push_back(Pair("result", result ? "successful" : "failed"));
                if(!result) {
                    statusObj.push_back(Pair("errorMessage", errorMessage));
                }
                break;
            }
        }

        if(!found && !found2) {
            statusObj.push_back(Pair("result", "failed"));
            statusObj.push_back(Pair("errorMessage", "could not find alias in config. Verify with list-conf."));
        }

        pwalletMain->Lock();
        return statusObj;

    }

    if (strCommand == "start-many")
    {
        if(pwalletMain->IsLocked()) {
            SecureString strWalletPass;
            strWalletPass.reserve(100);

            if (params.size() == 2){
                strWalletPass = params[1].get_str().c_str();
            } else {
                throw runtime_error(
                "Your wallet is locked, passphrase is required\n");
            }

            if(!pwalletMain->Unlock(strWalletPass)){
                return "incorrect passphrase";
            }
        }

        std::vector<CThroneConfig::CThroneEntry> mnEntries;
        mnEntries = throneConfig.getEntries();

        int total = 0;
        int successful = 0;
        int fail = 0;
        bool found = false;

        Object resultsObj;

        BOOST_FOREACH(CThroneConfig::CThroneEntry mne, throneConfig.getEntries()) {
            total++;

            std::string errorMessage;
            bool result;
            BOOST_FOREACH(CThrone& mn, vThrones) {
                std::string strAddr = mn.addr.ToString();
                if (strAddr == mne.getIp().ToString()){
                    found = true;
                }
                if (!found){
                    result = activeThrone.Register(mne.getIp(), mne.getPrivKey(), mne.getTxHash(), mne.getOutputIndex(), errorMessage);
                } else {
                    errorMessage = "Throne has already been started and your IP added to the list.";
                    result = false;
                }
            }

            Object statusObj;
            statusObj.push_back(Pair("alias", mne.getAlias()));
            statusObj.push_back(Pair("result", result ? "successful" : "failed"));

            if (result && !found) {
                successful++;
            } else if (!result && found){
                fail++;
                statusObj.push_back(Pair("errorMessage", errorMessage));
            } else {
                fail++;
                statusObj.push_back(Pair("errorMessage", errorMessage));
            }

            resultsObj.push_back(Pair("status", statusObj));
        }
        pwalletMain->Lock();

        Object returnObj;
        returnObj.push_back(Pair("overall", "Successfully started " + boost::lexical_cast<std::string>(successful) + " thrones, failed to start " +
                boost::lexical_cast<std::string>(fail) + ", total " + boost::lexical_cast<std::string>(total)));
        returnObj.push_back(Pair("detail", resultsObj));

        return returnObj;
    }

    if (strCommand == "debug")
    {
        if(activeThrone.status == THRONE_REMOTELY_ENABLED) return "throne started remotely";
        if(activeThrone.status == THRONE_INPUT_TOO_NEW) return "throne input must have at least 15 confirmations";
        if(activeThrone.status == THRONE_IS_CAPABLE) return "successfully started throne";
        if(activeThrone.status == THRONE_STOPPED) return "throne is stopped";
        if(activeThrone.status == THRONE_NOT_CAPABLE) return "not capable throne: " + activeThrone.notCapableReason;
        if(activeThrone.status == THRONE_SYNC_IN_PROCESS) return "sync in process. Must wait until client is synced to start.";

        CTxIn vin = CTxIn();
        CPubKey pubkey = CScript();
        CKey key;
        bool found = activeThrone.GetThroNeVin(vin, pubkey, key);
        if(!found){
            return "Missing throne input, please look at the documentation for instructions on throne creation";
        } else {
            return "No problems were found";
        }
    }

    if (strCommand == "create")
    {

        return "Not implemented yet, please look at the documentation for instructions on throne creation";
    }

    if (strCommand == "current")
    {
        CThrone* winner = mnodeman.GetCurrentThroNe(1);
        if(winner) {
            Object obj;
            CScript pubkey;
            pubkey.SetDestination(winner->pubkey.GetID());
            CTxDestination address1;
            ExtractDestination(pubkey, address1);
            CCrowncoinAddress address2(address1);

            obj.push_back(Pair("IP:port",       winner->addr.ToString().c_str()));
            obj.push_back(Pair("protocol",      (int64_t)winner->protocolVersion));
            obj.push_back(Pair("vin",           winner->vin.prevout.hash.ToString().c_str()));
            obj.push_back(Pair("pubkey",        address2.ToString().c_str()));
            obj.push_back(Pair("lastseen",      (int64_t)winner->lastTimeSeen));
            obj.push_back(Pair("activeseconds", (int64_t)(winner->lastTimeSeen - winner->sigTime)));
            return obj;
        }

        return "unknown";
    }

    if (strCommand == "genkey")
    {
        CKey secret;
        secret.MakeNewKey(false);

        return CCrowncoinSecret(secret).ToString();
    }

    if (strCommand == "winners")
    {
        Object obj;

        for(int nHeight = chainActive.Tip()->nHeight-10; nHeight < chainActive.Tip()->nHeight+20; nHeight++)
        {
            CScript payee;
            if(thronePayments.GetBlockPayee(nHeight, payee)){
                CTxDestination address1;
                ExtractDestination(payee, address1);
                CCrowncoinAddress address2(address1);
                obj.push_back(Pair(boost::lexical_cast<std::string>(nHeight),       address2.ToString().c_str()));
            } else {
                obj.push_back(Pair(boost::lexical_cast<std::string>(nHeight),       ""));
            }
        }

        return obj;
    }

    if(strCommand == "enforce")
    {
        return (uint64_t)enforceThronePaymentsTime;
    }

    if(strCommand == "connect")
    {
        std::string strAddress = "";
        if (params.size() == 2){
            strAddress = params[1].get_str().c_str();
        } else {
            throw runtime_error(
                "Throne address required\n");
        }

        CService addr = CService(strAddress);

        if(ConnectNode((CAddress)addr, NULL, true)){
            return "successfully connected";
        } else {
            return "error connecting";
        }
    }

    if(strCommand == "list-conf")
    {
    	std::vector<CThroneConfig::CThroneEntry> mnEntries;
    	mnEntries = throneConfig.getEntries();

        Object resultObj;

        BOOST_FOREACH(CThroneConfig::CThroneEntry mne, throneConfig.getEntries()) {
    		Object mnObj;
    		mnObj.push_back(Pair("alias", mne.getAlias()));
    		mnObj.push_back(Pair("address", mne.getIp()));
    		mnObj.push_back(Pair("privateKey", mne.getPrivKey()));
    		mnObj.push_back(Pair("txHash", mne.getTxHash()));
    		mnObj.push_back(Pair("outputIndex", mne.getOutputIndex()));
    		resultObj.push_back(Pair("throne", mnObj));
    	}

        return resultObj;
    }

    if (strCommand == "outputs"){
        // Find possible candidates
        vector<COutput> possibleCoins = activeThrone.SelectCoinsThrone();

        Object obj;
        BOOST_FOREACH(COutput& out, possibleCoins) {
            obj.push_back(Pair(out.tx->GetHash().ToString().c_str(), boost::lexical_cast<std::string>(out.i)));
        }

        return obj;

    }

    if(strCommand == "vote-many")
    {
        std::vector<CThroneConfig::CThroneEntry> mnEntries;
        mnEntries = throneConfig.getEntries();

        if (params.size() != 2)
            throw runtime_error("You can only vote 'yea' or 'nay'");

        std::string vote = params[1].get_str().c_str();
        if(vote != "yea" && vote != "nay") return "You can only vote 'yea' or 'nay'";
        int nVote = 0;
        if(vote == "yea") nVote = 1;
        if(vote == "nay") nVote = -1;


        int success = 0;
        int failed = 0;

        Object resultObj;

        BOOST_FOREACH(CThroneConfig::CThroneEntry mne, throneConfig.getEntries()) {
            std::string errorMessage;
            std::vector<unsigned char> vchThroNeSignature;
            std::string strThroNeSignMessage;

            CPubKey pubKeyCollateralAddress;
            CKey keyCollateralAddress;
            CPubKey pubKeyThrone;
            CKey keyThrone;

            if(!darkSendSigner.SetKey(mne.getPrivKey(), errorMessage, keyThrone, pubKeyThrone)){
                printf(" Error upon calling SetKey for %s\n", mne.getAlias().c_str());
                failed++;
                continue;
            }
            
            CThrone* pmn = mnodeman.Find(pubKeyThrone);
            if(pmn == NULL)
            {
                printf("Can't find throne by pubkey for %s\n", mne.getAlias().c_str());
                failed++;
                continue;
            }

            std::string strMessage = pmn->vin.ToString() + boost::lexical_cast<std::string>(nVote);

            if(!darkSendSigner.SignMessage(strMessage, errorMessage, vchThroNeSignature, keyThrone)){
                printf(" Error upon calling SignMessage for %s\n", mne.getAlias().c_str());
                failed++;
                continue;
            }

            if(!darkSendSigner.VerifyMessage(pubKeyThrone, vchThroNeSignature, strMessage, errorMessage)){
                printf(" Error upon calling VerifyMessage for %s\n", mne.getAlias().c_str());
                failed++;
                continue;
            }

            success++;

            //send to all peers
            LOCK(cs_vNodes);
            BOOST_FOREACH(CNode* pnode, vNodes)
                pnode->PushMessage("mvote", pmn->vin, vchThroNeSignature, nVote);
        }

        return("Voted successfully " + boost::lexical_cast<std::string>(success) + " time(s) and failed " + boost::lexical_cast<std::string>(failed) + " time(s).");
    }

    if(strCommand == "vote")
    {
        std::vector<CThroneConfig::CThroneEntry> mnEntries;
        mnEntries = throneConfig.getEntries();

        if (params.size() != 2)
            throw runtime_error("You can only vote 'yea' or 'nay'");

        std::string vote = params[1].get_str().c_str();
        if(vote != "yea" && vote != "nay") return "You can only vote 'yea' or 'nay'";
        int nVote = 0;
        if(vote == "yea") nVote = 1;
        if(vote == "nay") nVote = -1;

        // Choose coins to use
        CPubKey pubKeyCollateralAddress;
        CKey keyCollateralAddress;
        CPubKey pubKeyThrone;
        CKey keyThrone;

        std::string errorMessage;
        std::vector<unsigned char> vchThroNeSignature;
        std::string strMessage = activeThrone.vin.ToString() + boost::lexical_cast<std::string>(nVote);

        if(!darkSendSigner.SetKey(strThroNePrivKey, errorMessage, keyThrone, pubKeyThrone))
            return(" Error upon calling SetKey");

        if(!darkSendSigner.SignMessage(strMessage, errorMessage, vchThroNeSignature, keyThrone))
            return(" Error upon calling SignMessage");

        if(!darkSendSigner.VerifyMessage(pubKeyThrone, vchThroNeSignature, strMessage, errorMessage))
            return(" Error upon calling VerifyMessage");

        //send to all peers
        LOCK(cs_vNodes);
        BOOST_FOREACH(CNode* pnode, vNodes)
            pnode->PushMessage("mvote", activeThrone.vin, vchThroNeSignature, nVote);

    }

    return Value::null;
}

Value thronelist(const Array& params, bool fHelp)
{
    std::string strMode = "status";
    std::string strFilter = "";

    if (params.size() >= 1) strMode = params[0].get_str();
    if (params.size() == 2) strFilter = params[1].get_str();

    if (fHelp ||
            (strMode != "status" && strMode != "vin" && strMode != "pubkey" && strMode != "lastseen" && strMode != "activeseconds" && strMode != "rank"
                && strMode != "protocol" && strMode != "full" && strMode != "votes" && strMode != "donation" && strMode != "pose"))
    {
        throw runtime_error(
                "thronelist ( \"mode\" \"filter\" )\n"
                "Get a list of thrones in different modes\n"
                "\nArguments:\n"
                "1. \"mode\"      (string, optional/required to use filter, defaults = status) The mode to run list in\n"
                "2. \"filter\"    (string, optional) Filter results. Partial match by IP by default in all modes, additional matches in some modes\n"
                "\nAvailable modes:\n"
                "  activeseconds  - Print number of seconds throne recognized by the network as enabled\n"
                "  donation       - Show donation settings\n"
                "  full           - Print info in format 'status protocol pubkey vin lastseen activeseconds' (can be additionally filtered, partial match)\n"
                "  lastseen       - Print timestamp of when a throne was last seen on the network\n"
                "  pose           - Print Proof-of-Service score\n"
                "  protocol       - Print protocol of a throne (can be additionally filtered, exact match))\n"
                "  pubkey         - Print public key associated with a throne (can be additionally filtered, partial match)\n"
                "  rank           - Print rank of a throne based on current block\n"
                "  status         - Print throne status: ENABLED / EXPIRED / VIN_SPENT / REMOVE / POS_ERROR (can be additionally filtered, partial match)\n"
                "  vin            - Print vin associated with a throne (can be additionally filtered, partial match)\n"
                "  votes          - Print all throne votes for a Dash initiative (can be additionally filtered, partial match)\n"
                );
    }

    Object obj;
    if (strMode == "rank") {
        std::vector<pair<int, CThrone> > vThroneRanks = mnodeman.GetThroneRanks(chainActive.Tip()->nHeight);
        BOOST_FOREACH(PAIRTYPE(int, CThrone)& s, vThroneRanks) {
            std::string strAddr = s.second.addr.ToString();
            if(strFilter !="" && strAddr.find(strFilter) == string::npos) continue;
            obj.push_back(Pair(strAddr,       s.first));
        }
    } else {
        std::vector<CThrone> vThrones = mnodeman.GetFullThroneVector();
        BOOST_FOREACH(CThrone& mn, vThrones) {
            std::string strAddr = mn.addr.ToString();
            if (strMode == "activeseconds") {
                if(strFilter !="" && strAddr.find(strFilter) == string::npos) continue;
                obj.push_back(Pair(strAddr,       (int64_t)(mn.lastTimeSeen - mn.sigTime)));
            } else if (strMode == "full") {
                CScript pubkey;
                pubkey.SetDestination(mn.pubkey.GetID());
                CTxDestination address1;
                ExtractDestination(pubkey, address1);
                CCrowncoinAddress address2(address1);

                std::ostringstream addrStream;
                addrStream << setw(21) << strAddr;

                std::ostringstream stringStream;
                stringStream << setw(10) <<
                               mn.Status() << " " <<
                               mn.protocolVersion << " " <<
                               address2.ToString() << " " <<
                               mn.vin.prevout.hash.ToString() << " " <<
                               mn.lastTimeSeen << " " << setw(8) <<
                               (mn.lastTimeSeen - mn.sigTime);
                std::string output = stringStream.str();
                stringStream << " " << strAddr;
                if(strFilter !="" && stringStream.str().find(strFilter) == string::npos &&
                        strAddr.find(strFilter) == string::npos) continue;
                obj.push_back(Pair(addrStream.str(), output));
            } else if (strMode == "lastseen") {
                if(strFilter !="" && strAddr.find(strFilter) == string::npos) continue;
                obj.push_back(Pair(strAddr,       (int64_t)mn.lastTimeSeen));
            } else if (strMode == "protocol") {
                if(strFilter !="" && strFilter != boost::lexical_cast<std::string>(mn.protocolVersion) &&
                    strAddr.find(strFilter) == string::npos) continue;
                obj.push_back(Pair(strAddr,       (int64_t)mn.protocolVersion));
            } else if (strMode == "pubkey") {
                CScript pubkey;
                pubkey.SetDestination(mn.pubkey.GetID());
                CTxDestination address1;
                ExtractDestination(pubkey, address1);
                CCrowncoinAddress address2(address1);

                if(strFilter !="" && address2.ToString().find(strFilter) == string::npos &&
                    strAddr.find(strFilter) == string::npos) continue;
                obj.push_back(Pair(strAddr,       address2.ToString().c_str()));
            } else if (strMode == "pose") {
                if(strFilter !="" && strAddr.find(strFilter) == string::npos) continue;
                std::string strOut = boost::lexical_cast<std::string>(mn.nScanningErrorCount);
                obj.push_back(Pair(strAddr,       strOut.c_str()));
            } else if(strMode == "status") {
                std::string strStatus = mn.Status();
                if(strFilter !="" && strAddr.find(strFilter) == string::npos && strStatus.find(strFilter) == string::npos) continue;
                obj.push_back(Pair(strAddr,       strStatus.c_str()));
            } else if (strMode == "vin") {
                if(strFilter !="" && mn.vin.prevout.hash.ToString().find(strFilter) == string::npos &&
                    strAddr.find(strFilter) == string::npos) continue;
                obj.push_back(Pair(strAddr,       mn.vin.prevout.hash.ToString().c_str()));
            } else if(strMode == "votes"){
                std::string strStatus = "ABSTAIN";

                //voting lasts 7 days, ignore the last vote if it was older than that
                if((GetAdjustedTime() - mn.lastVote) < (60*60*8))
                {
                    if(mn.nVote == -1) strStatus = "NAY";
                    if(mn.nVote == 1) strStatus = "YEA";
                }

                if(strFilter !="" && (strAddr.find(strFilter) == string::npos && strStatus.find(strFilter) == string::npos)) continue;
                obj.push_back(Pair(strAddr,       strStatus.c_str()));
            }
        }
    }
    return obj;

}
