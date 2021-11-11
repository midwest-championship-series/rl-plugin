#include "Updater.h"
#include "bakkesmod/wrappers/PluginManagerWrapper.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctime>

BAKKESMOD_PLUGIN(Updater, "SOSIO Updater", "1.0.0", PERMISSION_ALL)

shared_ptr<CVarManagerWrapper> globalCvarManager;

string Updater::getLocalVersion() {
    string ver = "";
    ifstream in(baseDir + LR"(\version)");
    if (in.is_open()) {
        string line;
        getline(in, line);
        if (line.length() > 0) {
            LOG("Current version: " + line);
            ver = line;
        }
        in.close();
    }
    return ver;
}

string Updater::getUpdateUrl() {
    string url = "https://sosio.chezy.dev";
    ifstream in(baseDir + LR"(\path)");
    if (in.is_open()) {
        string line;
        getline(in, line);
        if (line.length() > 0) {
            LOG("Update URL: " + line);
            url = line;
        }
        in.close();
    }
    return url;
}

void Updater::setUpdateUrl(string url) {
    ofstream file(baseDir + LR"(\path)", ios_base::trunc);
    file << url << endl;
    file.close();
}

void Updater::onLoad()
{
    LOGC("Checking for updates...");
    
    curVersion = getLocalVersion();

    globalCvarManager = cvarManager;
    _wmkdir(baseDir.c_str());
    if (updateAvailable()) {
        LOG("Update available! " + newVersion);
        shared_ptr<ModalWrapper> modal = make_shared<ModalWrapper>(gameWrapper->CreateModal("SOSIO"));
        modal->AddButton("UPDATE", false, [&]() {
            LOG("User clicked update button");
            bool update = true;
            vector<shared_ptr<BakkesMod::Plugin::LoadedPlugin>>* plugins = gameWrapper->GetPluginManager().GetLoadedPlugins();
            for (int i = 0; i < plugins->size(); i++) {
                if (plugins->at(i)->_filename == "sosio") {
                    LOG("SOSIO running, showing error modal");
                    update = false;
                    shared_ptr<ModalWrapper> err = make_shared<ModalWrapper>(gameWrapper->CreateModal("SOSIO"));
                    err->AddButton("OK", true);
                    err->SetBody("SOSIO.dll was already loaded! Please restart your game and try updating again.");
                    err->ShowModal();
                }
            }
            if (update) {
                LOG("Starting up thread.");
                shared_ptr<ModalWrapper> updating = make_shared<ModalWrapper>(gameWrapper->CreateModal("SOSIO"));
                updating->SetBody("Updating...");
                updating->ShowModal();
                thread th([&, updating]() {
                    LOG("Downloading update");
                    download(downloadURL);
                    LOG("Copying to plugins");
                    CopyFile(saveDest.c_str(), pluginDest.c_str(), false);
                    gameWrapper->Execute([&, updating](GameWrapper*) {
                        LOG("Closing updating modal, done updating!");
                        updating->CloseModal();
                        shared_ptr<ModalWrapper> done = make_shared<ModalWrapper>(gameWrapper->CreateModal("SOSIO"));
                        done->SetBody("Done! Would you like to launch SOSIO now?");
                        done->AddButton("LAUNCH", false, [&]() {
                            LOG("Launching SOSIO");
                            cvarManager->executeCommand("plugin load sosio");
                        });
                        done->AddButton("CANCEL", true);
                        done->ShowModal();
                    });
                });
                th.detach();
            }
        });
        modal->AddButton("CANCEL", true);
        modal->SetBody("\nA new update is available! Do you want to update it now?\n\nNew Version: "+newVersionNum+"\n\n");
        modal->ShowModal();
    }


    cvarManager->setBind("F1", "sosio_launch");

    cvarManager->registerNotifier("sosio_launch", [&](vector<string> args) {
        bool found = false;
        vector<shared_ptr<BakkesMod::Plugin::LoadedPlugin>>* plugins = gameWrapper->GetPluginManager().GetLoadedPlugins();
        for (int i = 0; i < plugins->size(); i++) {
            if (plugins->at(i)->_filename == "sosio") {
                found = true;
            }
        }
        if (!found) {
            LOGC("SOSIO not running, launching");
            cvarManager->executeCommand("plugin load sosio");
            cvarManager->setBind("F3", "sosio_exit");
        }
    }, "Starts the main plugin.", PERMISSION_ALL);

    cvarManager->registerNotifier("sosio_exit", [&](vector<string> args) {
        bool found = false;
        vector<shared_ptr<BakkesMod::Plugin::LoadedPlugin>>* plugins = gameWrapper->GetPluginManager().GetLoadedPlugins();
        for (int i = 0; i < plugins->size(); i++) {
            if (plugins->at(i)->_filename == "sosio") {
                found = true;
            }
        }
        if (found) {
            LOGC("Exiting SOSIO");
            cvarManager->executeCommand("plugin unload sosio");
            cvarManager->setBind("F1", "sosio_launch");
        }
        }, "Closes the main plugin.", PERMISSION_ALL);

    cvarManager->registerNotifier("sosio_update_path", [&](vector<string> args) {
        if (args.size() >= 2) {
            LOGC("Updating update path to " + args.at(1));
            setUpdateUrl(args.at(1));
        }
        }, "Updates the path to check", PERMISSION_ALL);
}

void Updater::onUnload() {}

size_t write_data(void* ptr, size_t size, size_t nmemb, FILE* stream)
{
    size_t written;
    written = fwrite(ptr, size, nmemb, stream);
    return written;
}

size_t write_data_string(const char* in, size_t size, size_t num, string* out)
{
    const size_t totalBytes(size * num);
    out->append(in, totalBytes);
    return totalBytes;
}

bool Updater::updateAvailable() {
    CURL* curl = curl_easy_init();
    string url = getUpdateUrl();
    if (curl) {
        LOG("curl good, checking for updates");
        LOG(url);
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

        // Don't bother trying IPv6, which would increase DNS resolution time.
        curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);

        // Don't wait forever, time out after 10 seconds.
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10);

        // Follow HTTP redirects if necessary.
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 4L);

        // Response information.
        int httpCode(0);
        unique_ptr<string> httpData(new string());

        // Hook up data handling function.
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data_string);

        // Hook up data container (will be passed as the last parameter to the
        // callback handling function).  Can be any pointer type, since it will
        // internally be passed as a void pointer.
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, httpData.get());

        // Run our HTTP GET command, capture the HTTP response code, and clean up.
        CURLcode res = curl_easy_perform(curl);
        LOG("CURLcode: " + to_string(res));
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        curl_easy_cleanup(curl);

        LOG("httpCode: " + to_string(httpCode));
        LOG("httpData: "  + *httpData.get());

        if (httpCode == 200)
        {
            LOGC("Got successful response from " + url);

            json data = json::parse(httpData.get()->c_str());

            downloadURL = data["Location"];
            newVersion = data["VersionId"];
            newVersionNum = data["Metadata"]["version"];
            LOGC("Location: " + downloadURL);
            LOGC("Version: " + newVersion);
            LOGC("Version Number: " + newVersionNum);

            if (newVersion != curVersion) {
                return true;
            }
        }
    }
    return false;
}

bool Updater::download(const string& url) {
    CURL* curl = curl_easy_init();
    if (curl) {
        LOG("curl good, downloading " + url);
        FILE* fp;
        _wfopen_s(&fp, saveDest.c_str(), LR"(wb)");
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        /* we tell libcurl to follow redirection */
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1); //Prevent "longjmp causes uninitialized stack frame" bug
        curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "deflate");
        stringstream out;
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        /* Perform the request, res will get the return code */
        CURLcode res = curl_easy_perform(curl);
        /* Check for errors */
        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n",
                curl_easy_strerror(res));
        }
        fclose(fp);
        LOG("downloaded!");
        ofstream file(baseDir + LR"(\version)", ios_base::trunc);
        file << newVersion << endl;
        file.close();
        curl_easy_cleanup(curl);
        return true;
    }
    return false;
}

void Updater::log(string text, bool console) {
    ofstream file(baseDir + LR"(\log.txt)", ios_base::app);
    time_t now = time(0);
    tm* ltm = localtime(&now);
    string datestring = to_string(1 + ltm->tm_mon) + "/" + to_string(ltm->tm_mday) + "/" + to_string(1900 + ltm->tm_year) + " " + to_string(ltm->tm_hour) + ":" + to_string(ltm->tm_min) + ":" + to_string(ltm->tm_sec);

    file << "[" + datestring + "] [UPDATER] " + text << endl;
    file.close();
    if (console) {
        cvarManager->log(text);
    }
}