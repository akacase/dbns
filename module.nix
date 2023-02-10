{ config, lib, pkgs, ... }:

with lib;

let
  cfg = config.services.dbns;
  format = pkgs.formats.json { };
  configFile = format.generate "dbns.conf" cfg.settings;
in
{
  options = {
    services.dbns = {
      enable = mkEnableOption "dbns";

      user = mkOption {
        type = types.str;
        default = "dbns";
        description = "User under which dbns is ran.";
      };

      group = mkOption {
        type = types.str;
        default = "dbns";
        description = "Group under which dbns is ran.";
      };

      dataDir = mkOption {
        type = types.str;
        default = "/srv/dbns";
        description = ''
          Where to keep the dbns data.
        '';
      };

      ip = mkOption {
        type = types.str;
        default = "127.0.0.1";
        description = ''
          dbns API IP.
        '';
      };

      gcp = mkOption {
        default = { port = 3000; };
        type = format.type;
        example = literalExpression ''
            gcp = {
              port = 3000;
            };
        '';
        description = "gcp settings";
      };

      azure = mkOption {
        default = { port = 4000; };
        type = format.type;
        example = literalExpression ''
            azure = {
              port = 4000;
            };
        '';
        description = "azure settings";
      };

      hostname = mkOption {
        type = types.str;
        description = ''
          The definitive, public domain you will use for your instance.
        '';
        example = "dbns.yourdomain.net";
      };

      protocol = mkOption {
        type = types.enum [ "http" "https" ];
        default = "https";
        description = ''
          Web server protocol.
        '';
      };

      ssl = mkOption {
        type = types.bool;
        default = true;
        description = ''
          Force SSL : put this to 'false' when Let's Encrypt has problems calling 'http:' to check the domain
        '';
      };

      settings = mkOption {
        default = { };
        type = format.type;
        example = literalExpression ''
          {
            azureConfig = {
              grant_type = "client_credentials";
              resource = "https://azure.com";
            };
          };
        '';
        description = ''
          Declarative dbns configuration. 
        '';
      };

      settingsFile = mkOption {
        type = types.str;
        description = ''
          A JSON file that holds the settings for dbns. 
        '';
      };
    };
  };

  config = mkIf cfg.enable {

    users.users.dbns = mkIf (cfg.user == "dbns") { group = cfg.group; isSystemUser = true; };
    users.groups.dbns = mkIf (cfg.group == "dbns") { };

    services.nginx = {
      recommendedGzipSettings = true;
      recommendedOptimisation = true;
      recommendedProxySettings = true;
      recommendedTlsSettings = true;
      enable = true;
      virtualHosts."${cfg.hostname}" = {
        enableACME = cfg.protocol == "https";
        forceSSL = cfg.ssl;
        root = "${cfg.dataDir}";
        locations = {
          "/" = {
            root = "/var/www";
          };
          "/gcp" = {
            proxyPass = "http://${cfg.ip}:${toString cfg.gcp.port}/graphql";
          };
          "/azure" = {
            proxyPass = "http://${cfg.ip}:${toString cfg.azure.port}/graphql";
          };
        };
      };
    };

    systemd.tmpfiles.rules = [
      "d ${cfg.dataDir} 0700 ${cfg.user} ${cfg.group} - -"
    ];

    systemd.services.dbns = {
      description = "dbns";
      wantedBy = [ "multi-user.target" ];
      after = [ "network.target" ];
      serviceConfig = {
        StateDirectory = "dbns";
        StateDirectoryMode = "0700";
        Type = "simple";
        ExecStart = "${pkgs.dbns}/bin/dbns --config ${cfg.settingsFile}";
        ExecReload = "${pkgs.coreutils}/bin/kill -HUP $MAINPID";
        ExecStop = "${pkgs.coreutils}/bin/kill -SIGINT $MAINPID";
        Restart = "on-failure";

        User = cfg.user;
        Group = cfg.group;

        # Hardening
        CapabilityBoundingSet = "";
        LimitNOFILE = 800000;
        #LockPersonality = true;
        #MemoryDenyWriteExecute = true;
        NoNewPrivileges = true;
        PrivateDevices = true;
        PrivateTmp = true;
        PrivateUsers = true;
        ProcSubset = "pid";
        #ProtectClock = true;
        #ProtectControlGroups = true;
        #ProtectHome = true;
        #ProtectHostname = true;
        #ProtectKernelLogs = true;
        #ProtectKernelModules = true;
        #ProtectKernelTunables = true;
        #ProtectProc = "invisible";
        #ProtectSystem = "strict";
        ReadOnlyPaths = [ cfg.settingsFile ];
        ReadWritePaths = [ cfg.dataDir ];
        RestrictAddressFamilies = [ "AF_INET" "AF_INET6" ];
        RestrictNamespaces = true;
        RestrictRealtime = true;
        RestrictSUIDSGID = true;
        SystemCallFilter = [ "@system-service" "~@privileged" "~@resources" ];
        UMask = "0077";
      };
    };
  };

  meta = {
    maintainers = with lib.maintainers; [ ato ];
  };
}