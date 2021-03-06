#include "widget.h"
#include "ui_widget.h"
#include "message.h"
#include "utils.h"
#include "serialize.h"

// Processes the commands entered directly in the server, not the chat messages
void Widget::sendCmdLine()
{
    if (!enableGameServer)
    {
        logMessage("This is not a game server, commands are disabled");
        return;
    }

    QString str = ui->cmdLine->text();

    if (str == "clear")
    {
        ui->log->clear();
        return;
    }
    else if (str == "stop")
    {
        delete this;
        return;
    }
    else if (str == "listTcpPlayers")
    {
        for (int i=0; i<tcpPlayers.size(); i++)
        {
            Player* p = tcpPlayers[i];
            logMessage(p->name+" "+p->IP+":"+QString().setNum(p->port));
        }
        return;
    }
    else if (str.startsWith("setPeer"))
    {
        if (udpPlayers.size() == 1)
        {
            cmdPeer = udpPlayers[0];
            QString peerName = cmdPeer->IP + " " + QString().setNum(cmdPeer->port);
            logMessage(QString("UDP: Peer set to ").append(peerName));
            return;
        }

        str = str.right(str.size()-8);
        QStringList args = str.split(':');
        bool ok;
        if (args.size() != 2)
        {
            if (args.size() != 1)
            {
                logMessage("UDP: setPeer takes a pony id or ip:port combination");
                return;
            }
            quint16 id = args[0].toUInt(&ok);
            if (!ok)
            {
                logMessage("UDP: setPeer takes a pony id as argument");
                return;
            }
            for (int i=0; i<udpPlayers.size();i++)
            {
                if (udpPlayers[i]->pony.id == id)
                {
                    cmdPeer = Player::findPlayer(udpPlayers,udpPlayers[i]->IP, udpPlayers[i]->port);
                    logMessage(QString("UDP: Peer set to "+udpPlayers[i]->pony.name));
                    return;
                }
            }
            logMessage(QString("UDP: Peer not found (id ").append(args[0]).append(")"));
            return;
        }

        quint16 port = args[1].toUInt(&ok);
        if (!ok)
        {
            logMessage("UDP: setPeer takes a port as argument");
            return;
        }

        cmdPeer = Player::findPlayer(udpPlayers,args[0], port);
        if (cmdPeer->IP!="")
            logMessage(QString("UDP: Peer set to ").append(str));
        else
            logMessage(QString("UDP: Peer not found (").append(str).append(")"));
        return;
    }
    else if (str.startsWith("listPeers"))
    {
        if (str.size()<=10)
        {
            for (int i=0; i<win.udpPlayers.size();i++)
                win.logMessage(QString().setNum(win.udpPlayers[i]->pony.netviewId)
                               +"   "+win.udpPlayers[i]->pony.name
                               +"   "+win.udpPlayers[i]->IP
                               +":"+QString().setNum(win.udpPlayers[i]->port)
                               +"   "+QString().setNum((int)timestampNow()-win.udpPlayers[i]->lastPingTime)+"s");
            return;
        }
        str = str.right(str.size()-10);
        Scene* scene = findScene(str);
        if (scene->name.isEmpty())
            win.logMessage("Can't find scene");
        else
            for (int i=0; i<scene->players.size();i++)
                win.logMessage(win.udpPlayers[i]->IP
                               +":"+QString().setNum(win.udpPlayers[i]->port)
                               +" "+QString().setNum((int)timestampNow()-win.udpPlayers[i]->lastPingTime)+"s");
        return;
    }
    else if (str.startsWith("listVortexes"))
    {
        for (int i=0; i<scenes.size(); i++)
        {
            win.logMessage("Scene "+scenes[i].name);
            for (int j=0; j<scenes[i].vortexes.size(); j++)
                win.logMessage("Vortex "+QString().setNum(scenes[i].vortexes[j].id)
                               +" to "+scenes[i].vortexes[j].destName+" "
                               +QString().setNum(scenes[i].vortexes[j].destPos.x)+" "
                               +QString().setNum(scenes[i].vortexes[j].destPos.y)+" "
                               +QString().setNum(scenes[i].vortexes[j].destPos.z));
        }
        return;
    }
    else if (str.startsWith("sync"))
    {
        win.logMessage("UDP: Syncing manually");
        sync.doSync();
        return;
    }
    // DEBUG global commands from now on
    else if (str==("dbgStressLoad"))
    {
        // Send all the players to the GemMines at the same time
        for (int i=0; i<udpPlayers.size(); i++)
            sendLoadSceneRPC(udpPlayers[i], "GemMines");
        return;
    }
    else if (str.startsWith("dbgStressLoad"))
    {
        str = str.mid(14);
        // Send all the players to the given scene at the same time
        for (int i=0; i<udpPlayers.size(); i++)
            sendLoadSceneRPC(udpPlayers[i], str);
        return;
    }
    else if (str.startsWith("tele"))
    {
        str = str.right(str.size()-5);
        QStringList args = str.split(' ');
        if (args.size() != 2)
        {
            logStatusMessage("Error: Usage is tele ponyIdToMove destinationPonyId");
            return;
        }
        bool ok;
        bool ok1;
        bool ok2 = false;
        quint16 sourceID = args[0].toUInt(&ok);
        quint16 destID = args[1].toUInt(&ok1);
        Player* sourcePeer;
        if (!ok && !ok1)
        {
            logStatusMessage("Error: Usage is tele ponyIdToMove destinationPonyId");
            return;
        }
        for (int i=0; i<udpPlayers.size();i++)
        {
            if (udpPlayers[i]->pony.id == sourceID)
            {
                sourcePeer = udpPlayers[i];
                ok2 = true;
                break;
            }
        }
        if (!ok2)
        {
            logStatusMessage("Error: Source peer does not exist!");
            return;
        }
        for (int i=0; i<udpPlayers.size();i++)
        {
            if (udpPlayers[i]->pony.id == destID)
            {
                logMessage(QString("UDP: Teleported "+sourcePeer->pony.name+" to "+udpPlayers[i]->pony.name));
                if (udpPlayers[i]->pony.sceneName != sourcePeer->pony.sceneName)
                {
                    sendLoadSceneRPC(sourcePeer, udpPlayers[i]->pony.sceneName, udpPlayers[i]->pony.pos);
                }
                else
                {
                    sendMove(sourcePeer, udpPlayers[i]->pony.pos.x, udpPlayers[i]->pony.pos.y, udpPlayers[i]->pony.pos.z);
                }
                return;
            }
        }
        logMessage("Error: Destination peer does not exist!");
    }
    if (cmdPeer->IP=="")
    {
        logMessage("Select a peer first with setPeer/listPeers");
        return;
    }
    else // Refresh peer info
    {
        cmdPeer = Player::findPlayer(udpPlayers,cmdPeer->IP, cmdPeer->port);
        if (cmdPeer->IP=="")
        {
            logMessage(QString("UDP: Peer not found"));
            return;
        }
    }

    // User commands from now on (requires setPeer)
    if (str.startsWith("disconnect"))
    {
        logMessage(QString("UDP: Disconnecting"));
        sendMessage(cmdPeer,MsgDisconnect, "Connection closed by the server admin");
        Player::disconnectPlayerCleanup(cmdPeer); // Save game and remove the player
    }
    else if (str.startsWith("load"))
    {
        str = str.mid(5);
        sendLoadSceneRPC(cmdPeer, str);
    }
    else if (str.startsWith("getPos"))
    {
        logMessage(QString("Pos : x=") + QString().setNum(cmdPeer->pony.pos.x)
                   + ", y=" + QString().setNum(cmdPeer->pony.pos.y)
                   + ", z=" + QString().setNum(cmdPeer->pony.pos.z));
    }
    else if (str.startsWith("getRot"))
    {
        logMessage(QString("Rot : x=") + QString().setNum(cmdPeer->pony.rot.x)
                   + ", y=" + QString().setNum(cmdPeer->pony.rot.y)
                   + ", z=" + QString().setNum(cmdPeer->pony.rot.z)
                   + ", w=" + QString().setNum(cmdPeer->pony.rot.w));
    }
    else if (str.startsWith("getPonyData"))
    {
        logMessage("ponyData : "+cmdPeer->pony.ponyData.toBase64());
    }
    else if (str.startsWith("sendPonies"))
    {
        sendPonies(cmdPeer);
    }
    else if (str.startsWith("sendUtils3"))
    {
        logMessage("UDP: Sending Utils3 request");
        QByteArray data(1,3);
        sendMessage(cmdPeer,MsgUserReliableOrdered6,data);
    }
    else if (str.startsWith("setPlayerId"))
    {
        str = str.right(str.size()-12);
        QByteArray data(3,4);
        bool ok;
        unsigned id = str.toUInt(&ok);
        if (ok)
        {
            logMessage("UDP: Sending setPlayerId request");
            data[1]=(quint8)(id&0xFF);
            data[2]=(quint8)((id >> 8)&0xFF);
            sendMessage(cmdPeer,MsgUserReliableOrdered6,data);
        }
        else
            logStatusMessage("Error : Player ID is a number");
    }
    else if (str.startsWith("reloadNpc"))
    {
        str = str.mid(10);
        Pony* npc = NULL;
        for (int i=0; i<npcs.size(); i++)
            if (npcs[i]->name == str)
            {
                npc = npcs[i];
                break;
            }
        if (npc != NULL)
        {
            // Reload the NPCs from the DB
            npcs.clear();
            quests.clear();
            unsigned nQuests = 0;
            QDir npcsDir("data/npcs/");
            QStringList files = npcsDir.entryList(QDir::Files);
            for (int i=0; i<files.size(); i++, nQuests++) // For each vortex file
            {
                Quest *quest = new Quest("data/npcs/"+files[i], NULL);
                quests << *quest;
                npcs << quest->npc;
            }
            logMessage("Loaded "+QString().setNum(nQuests)+" quests/npcs.");

            // Resend the NPC if needed
            if (npc->sceneName == cmdPeer->pony.sceneName)
            {
                sendNetviewRemove(cmdPeer, npc->netviewId);
                sendNetviewInstantiate(npc, cmdPeer);
            }
        }
        else
            logMessage("NPC not found");
    }
    else if (str.startsWith("remove"))
    {
        str = str.right(str.size()-7);
        QByteArray data(3,2);
        bool ok;
        unsigned id = str.toUInt(&ok);
        if (ok)
        {
            logMessage("UDP: Sending remove request");
            data[1]=id;
            data[2]=id >> 8;
            sendMessage(cmdPeer,MsgUserReliableOrdered6,data);
        }
        else
            logStatusMessage("Error : Remove needs the id of the view to remove");
    }
    else if (str.startsWith("sendPonyData"))
    {
        QByteArray data(3,0xC8);
        data[0] = (quint8)(cmdPeer->pony.netviewId&0xFF);
        data[1] = (quint8)((cmdPeer->pony.netviewId>>8)&0xFF);
        data += cmdPeer->pony.ponyData;
        sendMessage(cmdPeer, MsgUserReliableOrdered18, data);
        return;
    }
    else if (str.startsWith("setStat"))
    {
        str = str.right(str.size()-8);
        QStringList args = str.split(' ');
        if (args.size() != 2)
        {
            logStatusMessage("Error : usage is setState StatID StatValue");
            return;
        }
        bool ok,ok2;
        quint8 statID = args[0].toInt(&ok);
        float statValue = args[1].toFloat(&ok2);
        if (!ok || !ok2)
        {
            logStatusMessage("Error : usage is setState StatID StatValue");
            return;
        }
        sendSetStatRPC(cmdPeer, statID, statValue);
    }
    else if (str.startsWith("setMaxStat"))
    {
        str = str.right(str.size()-11);
        QStringList args = str.split(' ');
        if (args.size() != 2)
        {
            logStatusMessage("Error : usage is setMaxStat StatID StatValue");
            return;
        }
        bool ok,ok2;
        quint8 statID = args[0].toInt(&ok);
        float statValue = args[1].toFloat(&ok2);
        if (!ok || !ok2)
        {
            logStatusMessage("Error : usage is setMaxState StatID StatValue");
            return;
        }
        sendSetMaxStatRPC(cmdPeer, statID, statValue);
    }
    else if (str.startsWith("instantiate"))
    {
        if (str == "instantiate")
        {
            logMessage("UDP: Instantiating");
            sendNetviewInstantiate(cmdPeer);
            return;
        }

        QByteArray data(1,1);
        str = str.right(str.size()-12);
        QStringList args = str.split(' ');

        if (args.size() != 3 && args.size() != 6 && args.size() != 10)
        {
            logStatusMessage(QString("Error : Instantiate takes 0,3,6 or 10 arguments").append(str));
            return;
        }
        // Au as au moins les 3 premiers de toute facon
        data += stringToData(args[0]);
        unsigned viewId, ownerId;
        bool ok1, ok2;
        viewId = args[1].toUInt(&ok1);
        ownerId = args[2].toUInt(&ok2);
        if (!ok1 || !ok2)
        {
            logStatusMessage(QString("Error : instantiate key viewId ownerId x1 y1 z1 x2 y2 z2 w2"));
            return;
        }
        QByteArray params1(4,0);
        params1[0] = (quint8)(viewId&0xFF);
        params1[1] = (quint8)((viewId >> 8)&0xFF);
        params1[2] = (quint8)(ownerId&0xFF);
        params1[3] = (quint8)((ownerId >> 8)&0xFF);
        data += params1;
        float x1=0,y1=0,z1=0;
        float x2=0,y2=0,z2=0,w2=0;
        if (args.size() >= 6) // Si on a le vecteur position on l'ajoute
        {
            bool ok1, ok2, ok3;
            x1=args[3].toFloat(&ok1);
            y1=args[4].toFloat(&ok2);
            z1=args[5].toFloat(&ok3);

            if (!ok1 || !ok2 || !ok3)
            {
                logStatusMessage(QString("Error : instantiate key viewId ownerId x1 y1 z1 x2 y2 z2 w2"));
                return;
            }
        }
        data+=floatToData(x1);
        data+=floatToData(y1);
        data+=floatToData(z1);

        if (args.size() == 10) // Si on a le quaternion rotation on l'ajoute
        {
            bool ok1, ok2, ok3, ok4;
            x2=args[6].toFloat(&ok1);
            y2=args[7].toFloat(&ok2);
            z2=args[8].toFloat(&ok3);
            w2=args[9].toFloat(&ok4);

            if (!ok1 || !ok2 || !ok3 || !ok4)
            {
                logStatusMessage(QString("Error : instantiate key viewId ownerId x1 y1 z1 x2 y2 z2 w2"));
                return;
            }
        }
        data+=floatToData(x2);
        data+=floatToData(y2);
        data+=floatToData(z2);
        data+=floatToData(w2);

        logMessage(QString("UDP: Instantiating ").append(args[0]));
        sendMessage(cmdPeer,MsgUserReliableOrdered6,data);
    }
    else if (str.startsWith("beginDialog"))
    {
        QByteArray data(1,0);
        data[0] = 11; // Request number

        sendMessage(cmdPeer,MsgUserReliableOrdered4, data);
    }
    else if (str.startsWith("endDialog"))
    {
        QByteArray data(1,0);
        data[0] = 13; // Request number

        sendMessage(cmdPeer,MsgUserReliableOrdered4, data);
    }
    else if (str.startsWith("setDialogMsg"))
    {
        str = str.right(str.size()-13);
        QStringList args = str.split(" ", QString::SkipEmptyParts);
        if (args.size() != 2)
            win.logMessage("setDialogMsg takes two args : dialog and npc name");
        else
        {
            QByteArray data(1,0);
            data[0] = 0x11; // Request number
            data += stringToData(args[0]);
            data += stringToData(args[1]);
            data += (char)0; // emoticon
            data += (char)0; // emoticon

            sendMessage(cmdPeer,MsgUserReliableOrdered4, data);
        }
    }
    else if (str.startsWith("setDialogOptions"))
    {
        str = str.right(str.size()-17);
        QStringList args = str.split(" ", QString::SkipEmptyParts);
        sendDialogOptions(cmdPeer, args);
    }
    else if (str.startsWith("move"))
    {
        str = str.right(str.size()-5);
        QByteArray data(1,0);
        data[0] = 0xce; // Request number

        // Serialization : float x, float y, float z
        QStringList coords = str.split(' ');
        if (coords.size() != 3)
            return;

        sendMove(cmdPeer, coords[0].toFloat(), coords[1].toFloat(), coords[2].toFloat());
    }
    else if (str.startsWith("error"))
    {
        str = str.right(str.size()-6);
        QByteArray data(1,0);
        data[0] = 0x7f; // Request number

        data += stringToData(str);

        sendMessage(cmdPeer,MsgUserReliableOrdered4, data);
    }
    else if (str==("listQuests"))
    {
        for (const Quest& quest : cmdPeer->pony.quests)
        {
            win.logMessage("Quest "+QString().setNum(quest.id)+" ("+*(quest.name)
                           +") : "+QString().setNum(quest.state));
        }
    }
    else if (str==("listInventory"))
    {
        for (const InventoryItem& item : cmdPeer->pony.inv)
        {
            win.logMessage("Item "+QString().setNum(item.id)+" (pos "+QString().setNum(item.index)
                           +") : "+QString().setNum(item.amount));
        }
    }
    else if (str==("listWorn"))
    {
        for (const WearableItem& item : cmdPeer->pony.worn)
        {
            win.logMessage("Item "+QString().setNum(item.id)+" : slot "+QString().setNum(item.index));
        }
    }
}
