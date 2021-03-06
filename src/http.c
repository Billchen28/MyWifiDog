/* vim: set et sw=4 ts=4 sts=4 : */
/********************************************************************\
 * This program is free software; you can redistribute it and/or    *
 * modify it under the terms of the GNU General Public License as   *
 * published by the Free Software Foundation; either version 2 of   *
 * the License, or (at your option) any later version.              *
 *                                                                  *
 * This program is distributed in the hope that it will be useful,  *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of   *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the    *
 * GNU General Public License for more details.                     *
 *                                                                  *
 * You should have received a copy of the GNU General Public License*
 * along with this program; if not, contact:                        *
 *                                                                  *
 * Free Software Foundation           Voice:  +1-617-542-5942       *
 * 59 Temple Place - Suite 330        Fax:    +1-617-542-2652       *
 * Boston, MA  02111-1307,  USA       gnu@gnu.org                   *
 *                                                                  *
 \********************************************************************/

/* $Id$ */
/** @file http.c
  @brief HTTP IO functions
  @author Copyright (C) 2004 Philippe April <papril777@yahoo.com>
  @author Copyright (C) 2007 Benoit Grégoire
  @author Copyright (C) 2007 David Bird <david@coova.com>

 */
/* Note that libcs other than GLIBC also use this macro to enable vasprintf */
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "httpd.h"

#include "safe.h"
#include "debug.h"
#include "conf.h"
#include "auth.h"
#include "firewall.h"
#include "http.h"
#include "client_list.h"
#include "common.h"
#include "centralserver.h"
#include "util.h"
#include "wd_util.h"

#include "../config.h"

#include "timer_engine.h"
#include "timer_obj.h"
#include "timer_task_type.h"


typedef struct {  
    char **str;     //the PChar of string array  
    size_t num;     //the number of string  
}IString;  

void releaseIString(IString* v) {
    int j = 0;
    for (j=0;j<v->num;j++) {
        char *c = v->str[j];
        if (c != NULL) {
            free(c);    
        }
    }
    free(v->str);
}

/** \Split string by a char 
 * 
 * \param  src:the string that you want to split 
 * \param  delim:split string by this char 
 * \param  istr:a srtuct to save string-array's PChar and string's amount. 
 * \return  whether or not to split string successfully 
 * 
 */  
int Split(char *src, char *delim, IString* istr)//split buf  
{  
    int i;  
    char *str = NULL, *p = NULL;  
  
    (*istr).num = 1;  
    str = (char*)calloc(strlen(src)+1,sizeof(char));  
    if (str == NULL) return 0;  
    (*istr).str = (char**)calloc(1,sizeof(char *));  
    if ((*istr).str == NULL) return 0;  
    strcpy(str,src);  
  
    p = strtok(str, delim);  
    (*istr).str[0] = (char*)calloc(strlen(p)+1,sizeof(char));  
    if ((*istr).str[0] == NULL) return 0;  
    strcpy((*istr).str[0],p);  
    for(i=1; p = strtok(NULL, delim); i++)  
    {  
        (*istr).num++;  
        (*istr).str = (char**)realloc((*istr).str,(i+1)*sizeof(char *));  
        if ((*istr).str == NULL) return 0;  
        (*istr).str[i] = (char*)calloc(strlen(p)+1,sizeof(char));  
        if ((*istr).str[0] == NULL) return 0;  
        strcpy((*istr).str[i],p);  
    }  
    free(str);  
    str = p = NULL;  
    return 1;  
}  


typedef struct extend_arg_st {
    char *ip;
//    char *mac;
}_extend_arg, *extend_arg_t;


void free_extend_arg(extend_arg_t v) {
    if (v->ip != NULL) {
        free(v->ip);
    }
    // if (v->mac != NULL) {
    //     free(v->mac);
    // }
}

extend_arg_t parseExtend(const char* extend_arg_src) {
    extend_arg_t ret = (extend_arg_t)malloc(sizeof(_extend_arg));
    ret->ip = safe_strdup(extend_arg_src);
//    ret->mac = NULL;
    // IString args_list;
    // if (Split(extend_arg_src,"&",&args_list)) {
    //     int i;
    //     for (i=0;i<args_list.num;i++) {
    //         IString k_v_list;
    //         int j = 0;
    //         if (Split(args_list.str[i],"=",&k_v_list) ) {
    //             if (k_v_list.num == 2) {
    //                 if (strcmp("ip", k_v_list.str[0]) == 0) {
    //                     ret->ip = k_v_list.str[1];
    //                     k_v_list.str[1] = NULL;
    //                 } else if (strcmp("mac", k_v_list.str[0]) == 0) {
    //                     ret->mac = k_v_list.str[1];
    //                     k_v_list.str[1] = NULL;
    //                 }
    //             }
    //         }
    //         releaseIString(&k_v_list);
    //     }
    //     releaseIString(&args_list);
    // }
    // if (ret->ip == NULL || ret->mac == NULL) {
    //     free_extend_arg(ret);
    //     ret = NULL;
    // }
    return ret;
}

/** The 404 handler is also responsible for redirecting to the auth server */
void
http_callback_404(httpd * webserver, request * r, int error_code)
{
    char tmp_url[MAX_BUF], *url, *mac;
    s_config *config = config_get_config();
    t_auth_serv *auth_server = get_auth_server();
    debug(LOG_INFO, "http_callback_404 ua:%s", r->request.user_agent);
    memset(tmp_url, 0, sizeof(tmp_url));
    int req_src = normal_req;
    if (strlen(r->request.user_agent) > 0) {
        char wx_ua[128] = "micromessenger";
        char *found = strstr(r->request.user_agent, wx_ua);
        if (found != NULL) {
            req_src = wx_req;
            debug(LOG_INFO, "Is wx req.");
        }
    }
    
    /* 
     * XXX Note the code below assumes that the client's request is a plain
     * http request to a standard port. At any rate, this handler is called only
     * if the internet/auth server is down so it's not a huge loss, but still.
     */
    snprintf(tmp_url, (sizeof(tmp_url) - 1), "http://%s%s%s%s",
             r->request.host, r->request.path, r->request.query[0] ? "?" : "", r->request.query);
    url = httpdUrlEncode(tmp_url);
    if (!is_online()) {
        /* The internet connection is down at the moment  - apologize and do not redirect anywhere */
        char *buf;
        safe_asprintf(&buf,
                      "<p>We apologize, but it seems that the internet connection that powers this hotspot is temporarily unavailable.</p>"
                      "<p>If at all possible, please notify the owners of this hotspot that the internet connection is out of service.</p>"
                      "<p>The maintainers of this network are aware of this disruption.  We hope that this situation will be resolved soon.</p>"
                      "<p>In a while please <a href='%s'>click here</a> to try your request again.</p>", tmp_url);

        send_http_page(r, "Uh oh! Internet access unavailable!", buf);
        free(buf);
        debug(LOG_INFO, "Sent %s an apology since I am not online - no point sending them to auth server",
              r->clientAddr);
    } else if (!is_auth_online()) {
        /* The auth server is down at the moment - apologize and do not redirect anywhere */
        char *buf;
        safe_asprintf(&buf,
                      "<p>We apologize, but it seems that we are currently unable to re-direct you to the login screen.</p>"
                      "<p>The maintainers of this network are aware of this disruption.  We hope that this situation will be resolved soon.</p>"
                      "<p>In a couple of minutes please <a href='%s'>click here</a> to try your request again.</p>",
                      tmp_url);

        send_http_page(r, "Uh oh! Login screen unavailable!", buf);
        free(buf);
        debug(LOG_INFO, "Sent %s an apology since auth server not online - no point sending them to auth server",
              r->clientAddr);
    } else {
         /* Re-direct them to auth server */
        char *urlFragment;
        if (!(mac = arp_get(r->clientAddr))) {
            /* We could not get their MAC address */
            debug(LOG_INFO, "Failed to retrieve MAC address for ip %s, so not putting in the login request",
                  r->clientAddr);
            if (req_src == wx_req) {
                char *authurl_temp;
                safe_asprintf(&authurl_temp, "http://%s:%d/wifidog/wx_auth", config->gw_address, config->gw_port);
                char *authurl = httpdUrlEncode(authurl_temp);
                free(authurl_temp);
                char *extend = httpdUrlEncode("extend_msg_for_wifi_dog_from_qscan");
                safe_asprintf(&urlFragment, "%sgw_address=%s&gw_port=%d&gw_id=%s&gw_mac=%s&ip=%s&url=%s&authurl=%s&extend=%s",
                                  auth_server->authserv_login_script_path_fragment, config->gw_address, config->gw_port,
                                  config->gw_id, config->gw_mac, r->clientAddr, url, authurl, extend);
                free(authurl);
                free(extend);
            } else {
                safe_asprintf(&urlFragment, "%sgw_address=%s&gw_port=%d&gw_id=%s&gw_mac=%s&ip=%s&url=%s",
                              auth_server->authserv_login_script_path_fragment, config->gw_address, config->gw_port,
                              config->gw_id, config->gw_mac, r->clientAddr, url);
            }
        } else {
            debug(LOG_INFO, "Got client MAC address for ip %s: %s", r->clientAddr, mac);
            if (req_src == wx_req) {
                char *authurl_temp;
                safe_asprintf(&authurl_temp, "http://%s:%d/wifidog/wx_auth", config->gw_address, config->gw_port);
                char *authurl = httpdUrlEncode(authurl_temp);
                free(authurl_temp);
                char *extend = httpdUrlEncode("extend_msg_for_wifi_dog_from_qscan");
                safe_asprintf(&urlFragment, "%sgw_address=%s&gw_port=%d&gw_id=%s&gw_mac=%s&ip=%s&mac=%s&url=%s&authurl=%s&extend=%s",
                              auth_server->authserv_login_script_path_fragment,
                              config->gw_address, config->gw_port, config->gw_id, config->gw_mac, r->clientAddr, mac, url, 
                              authurl, extend);
                free(authurl);
                free(extend);
            } else {
                safe_asprintf(&urlFragment, "%sgw_address=%s&gw_port=%d&gw_id=%s&gw_mac=%s&ip=%s&mac=%s&url=%s",
                              auth_server->authserv_login_script_path_fragment,
                              config->gw_address, config->gw_port, config->gw_id, config->gw_mac, r->clientAddr, mac, url);
            }
            free(mac);
        }
        // if host is not in whitelist, maybe not in conf or domain'IP changed, it will go to here.
        debug(LOG_INFO, "Check host %s is in whitelist or not", r->request.host);       // e.g. www.example.com
        t_firewall_rule *rule;
        //e.g. example.com is in whitelist
        // if request http://www.example.com/, it's not equal example.com.
        for (rule = get_ruleset("global"); rule != NULL; rule = rule->next) {
            debug(LOG_INFO, "rule mask %s", rule->mask);
            if (strstr(r->request.host, rule->mask) == NULL) {
                debug(LOG_INFO, "host %s is not in %s, continue", r->request.host, rule->mask);
                continue;
            }
            int host_length = strlen(r->request.host);
            int mask_length = strlen(rule->mask);
            if (host_length != mask_length) {
                char prefix[1024] = { 0 };
                // must be *.example.com, if not have ".", maybe Phishing. e.g. phishingexample.com
                strncpy(prefix, r->request.host, host_length - mask_length - 1);        // e.g. www
                strcat(prefix, ".");    // www.
                strcat(prefix, rule->mask);     // www.example.com
                if (strcasecmp(r->request.host, prefix) == 0) {
                    debug(LOG_INFO, "allow subdomain");
                    fw_allow_host(r->request.host);
                    http_send_redirect(r, tmp_url, "allow subdomain");
                    free(url);
                    free(urlFragment);
                    return;
                }
            } else {
                // e.g. "example.com" is in conf, so it had been parse to IP and added into "iptables allow" when wifidog start. but then its' A record(IP) changed, it will go to here.
                debug(LOG_INFO, "allow domain again, because IP changed");
                fw_allow_host(r->request.host);
                http_send_redirect(r, tmp_url, "allow domain");
                free(url);
                free(urlFragment);
                return;
            }
        }
        debug(LOG_INFO, "Captured %s requesting [%s] and re-directing them to login page", r->clientAddr, url);
        http_send_redirect_to_auth(r, urlFragment, "Redirect to login page");
        free(urlFragment);
    }
    free(url);
}

void
http_callback_wifidog(httpd * webserver, request * r)
{
    send_http_page(r, "WiFiDog", "Please use the menu to navigate the features of this WiFiDog installation.");
}

void
http_callback_about(httpd * webserver, request * r)
{
    send_http_page(r, "About WiFiDog", "This is WiFiDog version <strong>" VERSION "</strong>");
}

void
http_callback_status(httpd * webserver, request * r)
{
    const s_config *config = config_get_config();
    char *status = NULL;
    char *buf;

    if (config->httpdusername &&
        (strcmp(config->httpdusername, r->request.authUser) ||
         strcmp(config->httpdpassword, r->request.authPassword))) {
        debug(LOG_INFO, "Status page requested, forcing authentication");
        httpdForceAuthenticate(r, config->httpdrealm);
        return;
    }

    status = get_status_text();
    safe_asprintf(&buf, "<pre>%s</pre>", status);
    send_http_page(r, "WiFiDog Status", buf);
    free(buf);
    free(status);
}

/** @brief Convenience function to redirect the web browser to the auth server
 * @param r The request
 * @param urlFragment The end of the auth server URL to redirect to (the part after path)
 * @param text The text to include in the redirect header ant the mnual redirect title */
void
http_send_redirect_to_auth(request * r, const char *urlFragment, const char *text)
{
    char *protocol = NULL;
    int port = 80;
    t_auth_serv *auth_server = get_auth_server();

    if (auth_server->authserv_use_ssl) {
        protocol = "https";
        port = auth_server->authserv_ssl_port;
    } else {
        protocol = "http";
        port = auth_server->authserv_http_port;
    }

    char *url = NULL;
    safe_asprintf(&url, "%s://%s:%d%s%s",
                  protocol, auth_server->authserv_hostname, port, auth_server->authserv_path, urlFragment);
    http_send_redirect(r, url, text);
    free(url);
}

/** @brief Sends a redirect to the web browser 
 * @param r The request
 * @param url The url to redirect to
 * @param text The text to include in the redirect header and the manual redirect link title.  NULL is acceptable */
void
http_send_redirect(request * r, const char *url, const char *text)
{
    char *message = NULL;
    char *header = NULL;
    char *response = NULL;
    /* Re-direct them to auth server */
    debug(LOG_DEBUG, "Redirecting client browser to %s", url);
    safe_asprintf(&header, "Location: %s", url);
    safe_asprintf(&response, "302 %s\n", text ? text : "Redirecting");
    httpdSetResponse(r, response);
    httpdAddHeader(r, header);
    free(response);
    free(header);
    safe_asprintf(&message, "Please <a href='%s'>click here</a>.", url);
    send_http_page(r, text ? text : "Redirection to message", message);
    free(message);
}

void http_wx_auth(httpd *webserver, request *r){
    debug(LOG_DEBUG, "------start proce http_wx_auth----------");
    int success = 0;
    t_client *client;
    char *mac;
    extend_arg_t extend_arg = NULL;
    httpVar *extend = httpdGetVariableByName(r, "extend");
    if (extend != NULL && extend->value != NULL) {
//        const int BUF_SIZE = 512;
//        char buffer[BUF_SIZE];
//        _httpd_decode(extend->value, buffer, BUF_SIZE);
        extend_arg = parseExtend(extend->value);
        debug(LOG_DEBUG, "extend:%s",extend->value);
        if (extend_arg != NULL) {
            debug(LOG_DEBUG, "ip:%s",extend_arg->ip);
        } else {
            debug(LOG_DEBUG, "extend_arg = NULL");
        }
    } else {
        debug(LOG_DEBUG, "extend:NULL");
    }
    httpVar *tid = httpdGetVariableByName(r, "tid");
    if (tid != NULL) {
        debug(LOG_DEBUG, "tid:%s",tid->value);
    } else {
        debug(LOG_DEBUG, "tid:NULL");
    }
    httpVar *openId = httpdGetVariableByName(r, "openId");
    if (openId != NULL) {
        debug(LOG_DEBUG, "openId:%s",openId->value);
    } else {
        debug(LOG_DEBUG, "openId:NULL");
    }
    if (extend != NULL && tid != NULL && openId != NULL) {
        char *token = NULL;
        safe_asprintf(&token, "%s_%s_%s",
                openId, tid, extend);
        char *ip = safe_strdup(r->clientAddr);
        if (extend_arg != NULL && extend_arg->ip != NULL) {
            if (ip != NULL) {
                free(ip);
                ip = safe_strdup(extend_arg->ip);
            }
        }
        mac = arp_get(ip);
        if (mac == NULL) {
            mac = safe_strdup("");
            debug(LOG_ERR, "Failed to retrieve MAC address for ip %s", ip);
        }
        LOCK_CLIENT_LIST();
        if ((client = client_list_find(ip, mac)) == NULL) {
            debug(LOG_DEBUG, "wx_auth_New client for %s", ip);
            client = client_list_add(ip, mac, token, wx_auth_type);
        } else {
            client->auth_type = wx_auth_type;
            debug(LOG_DEBUG, "Client for %s is already in the client list", client->ip);
        }
        UNLOCK_CLIENT_LIST();
        int retCode = authenticate_client(r, 1);
        if (retCode == AUTH_ALLOWED) {
            success = 1;
        } else {
            success = 0;
        }
        free(mac);
        free(token);
        if (ip != NULL) {
            free(ip);
        }
    }
    if (success) {
        httpdOutput(r, "wx_auth_success.");
        debug(LOG_DEBUG, "wx_auth_success.");
    } else {
        const s_config *config = config_get_config();
        httpdForceAuthenticate(r, config->httpdrealm);
        debug(LOG_DEBUG, "wx_auth_fail.");
    }
    if (extend_arg != NULL) {
        free_extend_arg(extend_arg);
    }
    debug(LOG_DEBUG, "------end proce http_wx_auth----------");
}

typedef struct wx_temp_resume_st {
    char *ip;
    char *mac;
}_wx_temp_resume, *wx_temp_resume_t;

int wx_tmp_auth_callback(void *data, int type) {
    debug(LOG_DEBUG, "wx_tmp_auth_callback");
    if (wx_temp_auth == type) {
        wx_temp_resume_t t = (wx_temp_resume_t)data;
        debug(LOG_DEBUG, "logout_client ip:%s, mac:%s",t->ip, t->mac);
        t_client *client = NULL;
        if (t->ip != NULL && t->mac != NULL) {
            LOCK_CLIENT_LIST();
            client = client_list_find(t->ip, t->mac);
            if (client != NULL && client->auth_type == wx_temp_auth_type) {
                client->auth_type = normal_auth_type;
                showDebugInfo("resume wx_tmp_auth", client);
                logout_client(client);
            }
            free(t->ip);
            free(t->mac);
            UNLOCK_CLIENT_LIST();
        }
    }
    return 0;
}

void 
http_callback_wx_temp_auth(httpd *webserver, request *r)
{
    debug(LOG_DEBUG, "-------start http_callback_wx_temp_auth-------");
    t_client *client;
    httpVar *token;
    char *mac;
    httpVar *logout = httpdGetVariableByName(r, "logout");
    if ((token = httpdGetVariableByName(r, "token"))) {
        /* They supplied variable "token" */
        if (!(mac = arp_get(r->clientAddr))) {
            /* We could not get their MAC address */
            debug(LOG_ERR, "Failed to retrieve MAC address for ip %s", r->clientAddr);
            send_http_page(r, "WiFiDog Error", "Failed to retrieve your MAC address");
        } else {
            /* We have their MAC address */
            LOCK_CLIENT_LIST();
            if ((client = client_list_find(r->clientAddr, mac)) == NULL) {
                debug(LOG_DEBUG, "wx_tmp_auth_New client for %s", r->clientAddr);
                client = client_list_add(r->clientAddr, mac, token->value, wx_temp_auth_type);
            } else if (logout) {
                logout_client(client);
            } else {
                debug(LOG_DEBUG, "Client for %s is already in the client list", client->ip);
            }
            UNLOCK_CLIENT_LIST();
            if (!logout) { /* applies for case 1 and 3 from above if */
                if (client != NULL) {
                    showDebugInfo("start wx_tmp_auth", client);
                    wx_temp_resume_t resume = malloc(sizeof(_wx_temp_resume));
                    resume->ip = safe_strdup(client->ip);
                    resume->mac = safe_strdup(client->mac);
//                    timer_obj_t to = new_timer_obj(2 * 60 * 1000, wx_temp_auth, (void*)resume, wx_tmp_auth_callback);
                    timer_obj_t to = new_timer_obj(30 * 1000, wx_temp_auth, (void*)resume, wx_tmp_auth_callback);
                    appendTimerTask(to);
                    debug(LOG_DEBUG, "appendTimerTask token:%s auth_type:%d", client->token, client->auth_type);
                }
                int retCode = authenticate_client(r, 1);
                if (retCode == AUTH_ALLOWED) {
                    httpdOutput(r, "try{jsonpTmpAuthCallback({\"success\":true})}catch(e){};");
                } else {
                    httpdOutput(r, "try{jsonpTmpAuthCallback({\"success\":false})}catch(e){};");
                }
            }
            free(mac);
        }
    } else {
        /* They did not supply variable "token" */
        send_http_page(r, "wifidog, wx support.", "Invalid token");
        debug(LOG_DEBUG, "-------end http_callback_wx_temp_auth-------");
        return;
    }
    debug(LOG_DEBUG, "-------end http_callback_wx_temp_auth-------");
}

void
http_callback_auth(httpd * webserver, request * r)
{
    t_client *client;
    httpVar *token;
    char *mac;
    httpVar *logout = httpdGetVariableByName(r, "logout");

    if ((token = httpdGetVariableByName(r, "token"))) {
        /* They supplied variable "token" */
        if (!(mac = arp_get(r->clientAddr))) {
            /* We could not get their MAC address */
            debug(LOG_ERR, "Failed to retrieve MAC address for ip %s", r->clientAddr);
            send_http_page(r, "WiFiDog Error", "Failed to retrieve your MAC address");
        } else {
            /* We have their MAC address */
            LOCK_CLIENT_LIST();

            if ((client = client_list_find(r->clientAddr, mac)) == NULL) {
                debug(LOG_DEBUG, "New client for %s", r->clientAddr);
                client_list_add(r->clientAddr, mac, token->value, normal_auth_type);
            } else if (logout) {
                logout_client(client);
            } else {
                debug(LOG_DEBUG, "Client for %s is already in the client list", client->ip);
            }

            UNLOCK_CLIENT_LIST();
            if (!logout) { /* applies for case 1 and 3 from above if */
                authenticate_client(r, 0);
            }
            free(mac);
        }
    } else {
        /* They did not supply variable "token" */
        send_http_page(r, "WiFiDog error", "Invalid token");
    }
}

void
http_callback_disconnect(httpd * webserver, request * r)
{
    const s_config *config = config_get_config();
    /* XXX How do you change the status code for the response?? */
    httpVar *token = httpdGetVariableByName(r, "token");
    httpVar *mac = httpdGetVariableByName(r, "mac");

    if (config->httpdusername &&
        (strcmp(config->httpdusername, r->request.authUser) ||
         strcmp(config->httpdpassword, r->request.authPassword))) {
        debug(LOG_INFO, "Disconnect requested, forcing authentication");
        httpdForceAuthenticate(r, config->httpdrealm);
        return;
    }

    if (token && mac) {
        t_client *client;

        LOCK_CLIENT_LIST();
        client = client_list_find_by_mac(mac->value);

        if (!client || strcmp(client->token, token->value)) {
            UNLOCK_CLIENT_LIST();
            debug(LOG_INFO, "Disconnect %s with incorrect token %s", mac->value, token->value);
            httpdOutput(r, "Invalid token for MAC");
            return;
        }

        /* TODO: get current firewall counters */
        logout_client(client);
        UNLOCK_CLIENT_LIST();
        httpdOutput(r, "Disconnect success.");
    } else {
        debug(LOG_INFO, "Disconnect called without both token and MAC given");
        httpdOutput(r, "Both the token and MAC need to be specified");
        return;
    }

    return;
}

void
send_http_page(request * r, const char *title, const char *message)
{
    s_config *config = config_get_config();
    char *buffer;
    struct stat stat_info;
    int fd;
    ssize_t written;

    fd = open(config->htmlmsgfile, O_RDONLY);
    if (fd == -1) {
        debug(LOG_CRIT, "Failed to open HTML message file %s: %s", config->htmlmsgfile, strerror(errno));
        return;
    }

    if (fstat(fd, &stat_info) == -1) {
        debug(LOG_CRIT, "Failed to stat HTML message file: %s", strerror(errno));
        close(fd);
        return;
    }
    // Cast from long to unsigned int
    buffer = (char *)safe_malloc((size_t) stat_info.st_size + 1);
    written = read(fd, buffer, (size_t) stat_info.st_size);
    if (written == -1) {
        debug(LOG_CRIT, "Failed to read HTML message file: %s", strerror(errno));
        free(buffer);
        close(fd);
        return;
    }
    close(fd);

    buffer[written] = 0;
    httpdAddVariable(r, "title", title);
    httpdAddVariable(r, "message", message);
    httpdAddVariable(r, "nodeID", config->gw_id);
    httpdOutput(r, buffer);
    free(buffer);
}
