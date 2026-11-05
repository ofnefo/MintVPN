
#include "../include/configs/StrictAmsterdamRoute.h"
#undef NDEBUG
#include <cassert>
using namespace Configs::StrictAmsterdamRoute;

int main() {
 QJsonObject raw{{"_mintvpn_strict_amsterdam_outbound",42},{"final",42},{"rules",QJsonArray{}}};
 QJsonObject legacy{{"final",-2}}; QJsonObject empty;
 assert(prepare(legacy,empty,{},false,false).isEmpty());
 assert(legacy.value("final").toInt()==-2 && empty.isEmpty());
 QJsonObject core{{"outbounds",QJsonArray{QJsonObject{{"type","socks"},{"tag","ams"},{"server","127.0.0.1"},{"server_port",12345}}}}};
 auto missing=raw, c=core; assert(!prepare(missing,c,{},true,false).isEmpty());
 auto bypass=raw; bypass["rules"]=QJsonArray{QJsonObject{{"action","route"},{"outbound",-2}}};
 c=core; assert(!prepare(bypass,c,{{42,"ams"}},true,false).isEmpty());
 auto fall=raw; fall["final"]=-2; c=core; assert(!prepare(fall,c,{{42,"ams"}},true,false).isEmpty());
 c=core; auto extra=raw; extra["rule_set"]=QJsonArray{};
 assert(!prepare(extra,c,{{42,"ams"}},true,false).isEmpty());
 c=core; auto aux=raw; assert(!prepare(aux,c,{{42,"ams"}},true,true).isEmpty());
 auto ok=raw; c=core; assert(!prepare(ok,c,{{42,"ams"}},true,false).isEmpty());
 QJsonObject okSsh{{"type","ssh"},{"tag","ams"},{"server","100.64.0.1"},{"user","test"},{"private_key_path","E:/test/id"},{"host_key",QJsonArray{"ssh-ed25519 AAAA"}},{"host_key_algorithms",QJsonArray{"ssh-ed25519"}}};
 ok=raw; c["outbounds"]=QJsonArray{okSsh}; assert(prepare(ok,c,{{42,"ams"}},true,false).isEmpty());
 assert(!ok.contains("_mintvpn_strict_amsterdam_outbound"));
 auto dns=c.value("dns").toObject(); auto server=dns.value("servers").toArray().at(0).toObject();
 assert(server.value("type").toString()=="tcp" && server.value("detour").toString()=="ams");
 assert(server.value("server").toString()=="1.1.1.1" && dns.value("final").toString()=="mintvpn-ams-dns");
 c=core; auto notlocal=raw; c["outbounds"]=QJsonArray{QJsonObject{{"type","direct"},{"tag","ams"}}};
 assert(!prepare(notlocal,c,{{42,"ams"}},true,false).isEmpty());
 QJsonObject ssh{{"type","ssh"},{"tag","ams"},{"server","100.64.0.1"},{"server_port",22},{"user","test"},{"private_key_path","E:/test/id_ed25519"},{"host_key",QJsonArray{"ssh-ed25519 AAAA"}},{"host_key_algorithms",QJsonArray{"ssh-ed25519"}}};
 auto check=[&](QJsonObject o){ auto r=raw; QJsonObject x{{"outbounds",QJsonArray{o}}}; return prepare(r,x,{{42,"ams"}},true,false); };
 assert(check(ssh).isEmpty());
 auto bad=ssh; bad.remove("host_key"); assert(!check(bad).isEmpty());
 bad=ssh; bad["host_key_algorithms"]=QJsonArray{"ecdsa-sha2-nistp256"}; assert(!check(bad).isEmpty());
 bad=ssh; bad["server"]="8.8.8.8"; assert(!check(bad).isEmpty());
 bad=ssh; bad["private_key_path"]="relative"; assert(!check(bad).isEmpty());
 bad=ssh; bad["password"]="synthetic"; assert(!check(bad).isEmpty());
 bad=ssh; bad["detour"]="direct"; assert(!check(bad).isEmpty());
 QJsonObject selected{{"process_name",QJsonArray{"claude.exe"}},{"outbound",42}};
 QJsonObject fallback{{"domain_suffix",QJsonArray{"example.test"}},{"outbound",-2}};
 QJsonObject split{{"_mintvpn_amsterdam_process_outbound",42},{"final",-1},{"rules",QJsonArray{selected,fallback}}};
 QJsonObject splitCore{{"outbounds",QJsonArray{ssh}},{"dns",QJsonObject{{"final","original"}}}};
 const auto originalCore=splitCore;
 auto preserved=split; preserved.remove("_mintvpn_amsterdam_process_outbound");
 auto splitOk=split; assert(!prepare(splitOk,splitCore,{{42,"ams"}},true,false).isEmpty());
 assert(splitOk==split && splitCore==originalCore);
 auto invalid=split; invalid["rules"]=QJsonArray{fallback,selected};
 assert(!prepare(invalid,splitCore,{{42,"ams"}},true,false).isEmpty());
 invalid=split; invalid["rules"]=QJsonArray{QJsonObject{{"process_name",QJsonArray{"*.exe"}},{"outbound",42}}};
 assert(!prepare(invalid,splitCore,{{42,"ams"}},true,false).isEmpty());
 invalid=split; assert(!prepare(invalid,splitCore,{},true,false).isEmpty());
 invalid=split; assert(!prepare(invalid,splitCore,{{42,"ams"}},true,true).isEmpty());
 return 0;
}
