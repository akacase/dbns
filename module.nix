{ config, lib, pkgs, ... }:

with lib;

let
  cfg = config.services.dbns;
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

      port = mkOption {
        default = 3000;
        type = format.type;
        example = literalExpression ''
              port = 3000;
        '';
        description = "port";
      };
    };
  };

  config = mkIf cfg.enable {

    users.users.dbns = mkIf (cfg.user == "dbns") { group = cfg.group; isSystemUser = true; };
    users.groups.dbns = mkIf (cfg.group == "dbns") { };

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
        ExecStart = "${pkgs.dbns}/bin/dbns -p ${cfg.port} -d /srv/dbns";
        ExecReload = "${pkgs.coreutils}/bin/kill -HUP $MAINPID";
        ExecStop = "${pkgs.coreutils}/bin/kill -SIGINT $MAINPID";
        Restart = "on-failure";

        User = cfg.user;
        Group = cfg.group;

        # Hardening
        CapabilityBoundingSet = "";
        LimitNOFILE = 800000;
        NoNewPrivileges = true;
        PrivateDevices = true;
        PrivateTmp = true;
        PrivateUsers = true;
        ProcSubset = "pid";
        ReadOnlyPaths = [ cfg.settingsFile ];
        ReadWritePaths = [ cfg.dataDir ];
        RestrictAddressFamilies = [ "AF_INET" "AF_INET6" ];
        RestrictNamespaces = true;
        RestrictRealtime = true;
        RestrictSUIDSGID = true;
        SystemCallFilter = [ "@system-service" "~@privileged" "~@resources" ];
      };
    };
  };

  meta = {
    maintainers = with lib.maintainers; [ dbns ];
  };
}