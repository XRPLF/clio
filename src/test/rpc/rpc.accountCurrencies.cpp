#include <algorithm>
#include <clio/backend/BackendFactory.h>
#include <clio/backend/DBHelpers.h>
#include <clio/rpc/RPCHelpers.h>
#include <gtest/gtest.h>

#include <test/env/env.h>

#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>


void
writeAccountCurrencies(
    Backend::BackendInterface& backend,
    boost::asio::yield_context& yield)
{
    std::string accountIndex = "B7D526FDDF9E3B3F95C3DC97C353065B0482302500BBB8051A5C090B596C6133";

    std::string account = "110061220000000024000000012501C1F1FE2D00000000554F9B612F6AD7CF041CA672F3E6B6CC5B13EB00F5E55A7D76C7C1CFB8B2C891C3624000000005F767A181140A20B3C85F482532A9578DBB3950B85CA06594D1";

    // These are just dummy data that will be owned by the account `account`.
    // They will contain lines, channels, escrows, 
    std::unordered_map<std::string, std::string> const accountHexBlobs = {
        {"7E1247F78EFC74FA9C0AE39F37AF433966615EB9B757D8397C068C2849A8F4A5", "1100642200000000310000000000000001320000000000000011587E1247F78EFC74FA9C0AE39F37AF433966615EB9B757D8397C068C2849A8F4A582140A20B3C85F482532A9578DBB3950B85CA06594D10113C41F029E1E503BF268D75721808258FCFF14EB44F6216D2836CFEE4032640418554505737B01C4C155942D34441894E86D88FFE7BF9B496DFFE8AF3C839E2473E3CF08E417C1C1F1EB461EC155C4C91E37D8E0031E937427625ACEE8F57A24DE3ED40FFAC9D22DA6F86775FCAAC9EC3843D148B8968E113981501C6B9CE107A8AB141133B692D0950BA7DB593CD76A92D3E3FD3BB7C9F2BDF3740B7D329BA40AA16913CBA83F632029C5A1299882DF390B3389E788A6AB46204DD8E718DA0E538C2D173A57E4272B99AD77B24C8B9B92FE382CE1256B81A4733BC8F345CBC666FDB11B38184A2C5D97DD48A36F85DC80357CD47C07FF1ABCA1FC8AE9F2A11635C24B22520594E15BBE4B76FBB724525B05A64F335F57A2BCADDEF9C2EA3216AF7F8D276DB9B772F02422CF5F7B02699B36193C9D68B15698DF5235EB457857592DEA290F2A000F4C80A096BF291A64695D0901858020190C4D6BE64799E67F606C8736CF40E1396967D3CB45C87D3E5EBDEB2B2E2F6BC31C6FD354193DE076476E5E3B6F94199BC2A0058710296C4CD43E63EAF561D7A233AFBF461372A99A8B942D3E229B0368522659C226C4315A4FFBE88300A1C858CCCAD2F29AB924F03D61DF5399A926E0522F5404A6AA08D06F4CBABBF9CE7533B50193B807145D9A71C90154F92FC0656997F902B7EB62DE6F6D44C41BBA985110266079345E2C35BF8AF159040EEC849FA3E1C7961BDC98F4292E55AA69AF0C20D3D0C3FDB52BB0ECD5E570ED9007F287FF4B45F6051C7B41F930ECBBD3672A1ADAF5E749F9AC63058CF774FF441C31DD16ABF769358E987922B42164D8FEAD7AF4B887CE91E362354B9D89A6AC29B8A555E301E60E0F776D3F4D702AD673D971B2DAC40A606E4518F2E48DE7A057F04DE9FAF8B39D5F41A9C214A71FC49ECFFEBA0A4AE41D636D4B97318F338D24CEC539BA6BD046C91B298C155E4A6751339EFFDD6DB75A9D2C912056AB7FF2443BCB33302AF0AED781518161C02493D6C77AAF3326B40FB644026C67AC09A2D372B0D82B63C29A8237AA8DAC6A39C23C18EB564D0BF008598B2B7056B21217716EB5F7A7264114DCD9B64B4625B7C063C02B791829CC8EA2F73C1EC6CFD2EF4D26701D78CD0F973D11AFAE54D7776FE3AC771A9EF496311B0DFF9CC7E2F8B72F157DE5E4BD4A0B53B15E7402CB0DFE285308C209122D4B4B05CE4534F1C0F8517663FD4F0DE1E244EF530C0F67F097B04E37B86BFE93C9A8406CE085F619374F8E352A523B99BD10E0555C0724A958D48BBC8F58B232A8BBBFD87629F9E0F01515D9FDE99FB77AE4CB73D819079F9D41D43A6483A52E981DE8DE144BFF0669FD99C7E22F11B6F14EBE1A6C0D311AC7E681F8CDFD0BB3C7ACA7E8FD38"},
        {"029E1E503BF268D75721808258FCFF14EB44F6216D2836CFEE40326404185545", "1100722200320000250057652E37000000000000000038000000000000000055D853C597E16F6EB76867378D558C4290BA237244C0EF8B5F4338161C5587A9076280000000000000000000000000000000000000005553440000000000000000000000000000000000000000000000000166800000000000000000000000000000000000000055534400000000000A20B3C85F482532A9578DBB3950B85CA06594D167D6A386F26F2869800000000000000000000000005553440000000000DE93655447ACDE79799135B0F2AEB9818C6BF356"},
        {"05737B01C4C155942D34441894E86D88FFE7BF9B496DFFE8AF3C839E2473E3CF", "11007822000000002503B1841C20270000003C34000000000000000039000000000000000055711C4F606C63076137FAE90ADC36379D7066CF551E96DA6FE2BDAB5ECBFACF2B6140000000000003E8624000000000000000712103CFD18E689434F032A4E84C63E2A3A6472D684EAF4FD52CA67742F3E24BAE81B281144B4E9C06F24296074F7BC48F92A97916C6DC5EA983143E9D4A2B8AA0780F682D136F7A56D6724EF53754"},
        {"08E417C1C1F1EB461EC155C4C91E37D8E0031E937427625ACEE8F57A24DE3ED4", "110072220032000025009BF54737000000000000000038000000000000000055AF8493225AA94D822A1C1C138E308B364AD8BA03320FC38A004C700D300D87406280000000000000000000000000000000000000005553440000000000000000000000000000000000000000000000000166800000000000000000000000000000000000000055534400000000000A20B3C85F482532A9578DBB3950B85CA06594D167D6A386F26F28698000000000000000000000000055534400000000008B2CC364EBA4E91A92486340C644D1449EC23504"},
        {"0FFAC9D22DA6F86775FCAAC9EC3843D148B8968E113981501C6B9CE107A8AB14", "11006F2200000000240000016725011B2244330000000000000000340000000000000000552B44EBE00728D04658E597A85EC4F71D20503B31ABBF556764AD8F7A80BA72F650105A70682882F317175860A1188FAFF2757537D1A66B8500894E038D7EA4C6800064D5038D7EA4C68000000000000000000000000000464F4F00000000007908A7F0EDD48EA896C3580A399F0EE78611C8E365400000003B9ACA0081144B4E9C06F24296074F7BC48F92A97916C6DC5EA9"},
        {"1133B692D0950BA7DB593CD76A92D3E3FD3BB7C9F2BDF3740B7D329BA40AA169", "110072220032000025006ED8A33700000000000000003800000000000000005535978BA42F6634CE52191B450E5B84C5414A8C6B3B1A9303ADED93788D8D89B86280000000000000000000000000000000000000005553440000000000000000000000000000000000000000000000000166800000000000000000000000000000000000000055534400000000000A20B3C85F482532A9578DBB3950B85CA06594D167D6A386F26F286980000000000000000000000000555344000000000043617972E78D697CC3BBD4EE88E2D16D2FAE0ECD"},
        {"13CBA83F632029C5A1299882DF390B3389E788A6AB46204DD8E718DA0E538C2D", "110072220032000025009D29BF37000000000000000038000000000000000055C6FA7D2FB49CED0C15CC33DEF73EF53681308AB033A138D2749AD8D627A99E086280000000000000000000000000000000000000005553440000000000000000000000000000000000000000000000000166800000000000000000000000000000000000000055534400000000000A20B3C85F482532A9578DBB3950B85CA06594D167D6A386F26F28698000000000000000000000000055534400000000004629E593DF48D41903E9D436D6C4F9E85FFF73EC"},
        {"173A57E4272B99AD77B24C8B9B92FE382CE1256B81A4733BC8F345CBC666FDB1", "1100752200000000250235ED022025E2A01BAC34000000000000000155470DC630B0251FCCA740EA9CA7781A502608890FB123F14D0C4BE20C4D5D44DB614000000005F21070701127A0258020085D1EDF8045DC7F6E42B8DB09E05248502852FB3FF27815FC94433D1CB621988101208114D4687456F5A08BEA4E7008FFBE27C7D76CF1F02E8314D4687456F5A08BEA4E7008FFBE27C7D76CF1F02E"},
        {"1B38184A2C5D97DD48A36F85DC80357CD47C07FF1ABCA1FC8AE9F2A11635C24B", "110072220032000025009553C5370000000000000000380000000000000000555A2171CBDE6532B7617B63F5BADCD198103999FAD93B237631A6610E442E8CD66280000000000000000000000000000000000000005553440000000000000000000000000000000000000000000000000166800000000000000000000000000000000000000055534400000000000A20B3C85F482532A9578DBB3950B85CA06594D167D6C38D7EA4C680000000000000000000000000005553440000000000FB810A75EB621C2884DDF82F60BC452FCCDEEAA9"},
        {"22520594E15BBE4B76FBB724525B05A64F335F57A2BCADDEF9C2EA3216AF7F8D", "110072220032000025007FC5E4370000000000000000380000000000000000558E879962ACE38EAF7D7E2497965A73F962057903994C598F0632C66ACF6360E56280000000000000000000000000000000000000004254430000000000000000000000000000000000000000000000000166800000000000000000000000000000000000000042544300000000000A20B3C85F482532A9578DBB3950B85CA06594D167D6A386F26F286980000000000000000000000000425443000000000071D3000DB576F7F41296A7FD4F1E4E81251AEB46"},
        {"276DB9B772F02422CF5F7B02699B36193C9D68B15698DF5235EB457857592DEA", "1100722200320000250074C5DC370000000000000000380000000000000000550E9ED7D85CC79964E2E403624F12BABA3E620B1A2EC889BF7B330BB894425C7F6280000000000000000000000000000000000000005553440000000000000000000000000000000000000000000000000166800000000000000000000000000000000000000055534400000000000A20B3C85F482532A9578DBB3950B85CA06594D167D6A386F26F2869800000000000000000000000005553440000000000338EF7E16D8BA204B2A9CBE37D7779D10A997D33"},
        {"7E1247F78EFC74FA9C0AE39F37AF433966615EB9B757D8397C068C2849A8F4A5", "1100642200000000310000000000000000320000000000000001587E1247F78EFC74FA9C0AE39F37AF433966615EB9B757D8397C068C2849A8F4A582140A20B3C85F482532A9578DBB3950B85CA06594D10113C41F029E1E503BF268D75721808258FCFF14EB44F6216D2836CFEE4032640418554505737B01C4C155942D34441894E86D88FFE7BF9B496DFFE8AF3C839E2473E3CF08E417C1C1F1EB461EC155C4C91E37D8E0031E937427625ACEE8F57A24DE3ED40FFAC9D22DA6F86775FCAAC9EC3843D148B8968E113981501C6B9CE107A8AB141133B692D0950BA7DB593CD76A92D3E3FD3BB7C9F2BDF3740B7D329BA40AA16913CBA83F632029C5A1299882DF390B3389E788A6AB46204DD8E718DA0E538C2D173A57E4272B99AD77B24C8B9B92FE382CE1256B81A4733BC8F345CBC666FDB11B38184A2C5D97DD48A36F85DC80357CD47C07FF1ABCA1FC8AE9F2A11635C24B22520594E15BBE4B76FBB724525B05A64F335F57A2BCADDEF9C2EA3216AF7F8D276DB9B772F02422CF5F7B02699B36193C9D68B15698DF5235EB457857592DEA290F2A000F4C80A096BF291A64695D0901858020190C4D6BE64799E67F606C8736CF40E1396967D3CB45C87D3E5EBDEB2B2E2F6BC31C6FD354193DE076476E5E3B6F94199BC2A0058710296C4CD43E63EAF561D7A233AFBF461372A99A8B942D3E229B0368522659C226C4315A4FFBE88300A1C858CCCAD2F29AB924F03D61DF5399A926E0522F5404A6AA08D06F4CBABBF9CE7533B50193B807145D9A71C90154F92FC0656997F902B7EB62DE6F6D44C41BBA985110266079345E2C35BF8AF159040EEC849FA3E1C7961BDC98F4292E55AA69AF0C20D3D0C3FDB52BB0ECD5E570ED9007F287FF4B45F6051C7B41F930ECBBD3672A1ADAF5E749F9AC63058CF774FF441C31DD16ABF769358E987922B42164D8FEAD7AF4B887CE91E362354B9D89A6AC29B8A555E301E60E0F776D3F4D702AD673D971B2DAC40A606E4518F2E48DE7A057F04DE9FAF8B39D5F41A9C214A71FC49ECFFEBA0A4AE41D636D4B97318F338D24CEC539BA6BD046C91B298C155E4A6751339EFFDD6DB75A9D2C912056AB7FF2443BCB33302AF0AED781518161C02493D6C77AAF3326B40FB644026C67AC09A2D372B0D82B63C29A8237AA8DAC6A39C23C18EB564D0BF008598B2B7056B21217716EB5F7A7264114DCD9B64B4625B7C063C02B791829CC8EA2F73C1EC6CFD2EF4D26701D78CD0F973D11AFAE54D7776FE3AC771A9EF496311B0DFF9CC7E2F8B72F157DE5E4BD4A0B53B15E7402CB0DFE285308C209122D4B4B05CE4534F1C0F8517663FD4F0DE1E244EF530C0F67F097B04E37B86BFE93C9A8406CE085F619374F8E352A523B99BD10E0555C0724A958D48BBC8F58B232A8BBBFD87629F9E0F01515D9FDE99FB77AE4CB73D819079F9D41D43A6483A52E981DE8DE144BFF0669FD99C7E22F11B6F14EBE1A6C0D311AC7E681F8CDFD0BB3C7ACA7E8FD38"},
        {"290F2A000F4C80A096BF291A64695D0901858020190C4D6BE64799E67F606C87", "1100722200320000250059BA9237000000000000000038000000000000000055EC32758465DACF8BAFC870479FAFDF8F572FA481BF41ECE776800FA09CAE4D6C6280000000000000000000000000000000000000005553440000000000000000000000000000000000000000000000000166800000000000000000000000000000000000000055534400000000000A20B3C85F482532A9578DBB3950B85CA06594D167D6A386F26F2869800000000000000000000000005553440000000000F2CF0F33662BA302E72DB0D0CBE00851C0778A4B"},
        {"36CF40E1396967D3CB45C87D3E5EBDEB2B2E2F6BC31C6FD354193DE076476E5E", "110072220032000025006EDC2F370000000000000000380000000000000000551AA25A563FFDD1DFF0BA4A563BE26EC0AE8416341C6901B1FF00C15FA5D5169D6280000000000000000000000000000000000000005553440000000000000000000000000000000000000000000000000166800000000000000000000000000000000000000055534400000000000A20B3C85F482532A9578DBB3950B85CA06594D167D6A386F26F286980000000000000000000000000555344000000000059FDFDFD03E892B221508B6EF8D37AA5A8BDCD88"},
        {"3B6F94199BC2A0058710296C4CD43E63EAF561D7A233AFBF461372A99A8B942D", "11006F2200000000240000016725011B2244330000000000000000340000000000000000552B44EBE00728D04658E597A85EC4F71D20503B31ABBF556764AD8F7A80BA72F650105A70682882F317175860A1188FAFF2757537D1A66B8500894E038D7EA4C6800064D5038D7EA4C68000000000000000000000000000464F4F00000000007908A7F0EDD48EA896C3580A399F0EE78611C8E365400000003B9ACA0081144B4E9C06F24296074F7BC48F92A97916C6DC5EA9"},
        {"3E229B0368522659C226C4315A4FFBE88300A1C858CCCAD2F29AB924F03D61DF", "11007822000000002503B1841C20270000003C34000000000000000039000000000000000055711C4F606C63076137FAE90ADC36379D7066CF551E96DA6FE2BDAB5ECBFACF2B6140000000000003E8624000000000000000712103CFD18E689434F032A4E84C63E2A3A6472D684EAF4FD52CA67742F3E24BAE81B281144B4E9C06F24296074F7BC48F92A97916C6DC5EA983143E9D4A2B8AA0780F682D136F7A56D6724EF53754"},
        {"5399A926E0522F5404A6AA08D06F4CBABBF9CE7533B50193B807145D9A71C901", "11007822000000002503B1841C20270000003C34000000000000000039000000000000000055711C4F606C63076137FAE90ADC36379D7066CF551E96DA6FE2BDAB5ECBFACF2B6140000000000003E8624000000000000000712103CFD18E689434F032A4E84C63E2A3A6472D684EAF4FD52CA67742F3E24BAE81B281144B4E9C06F24296074F7BC48F92A97916C6DC5EA983143E9D4A2B8AA0780F682D136F7A56D6724EF53754"},
        {"54F92FC0656997F902B7EB62DE6F6D44C41BBA985110266079345E2C35BF8AF1", "1100722200320000250059BA97370000000000000000380000000000000000553D5E64E96122EC1D942C47D1950DEE8FF0235A7517DF6CA47E170C22A9B028BE6280000000000000000000000000000000000000004254430000000000000000000000000000000000000000000000000166800000000000000000000000000000000000000042544300000000000A20B3C85F482532A9578DBB3950B85CA06594D167D6A386F26F2869800000000000000000000000004254430000000000F2CF0F33662BA302E72DB0D0CBE00851C0778A4B"},
        {"59040EEC849FA3E1C7961BDC98F4292E55AA69AF0C20D3D0C3FDB52BB0ECD5E5", "1100752200000000250235ED022025E2A01BAC34000000000000000155470DC630B0251FCCA740EA9CA7781A502608890FB123F14D0C4BE20C4D5D44DB614000000005F21070701127A0258020085D1EDF8045DC7F6E42B8DB09E05248502852FB3FF27815FC94433D1CB621988101208114D4687456F5A08BEA4E7008FFBE27C7D76CF1F02E8314D4687456F5A08BEA4E7008FFBE27C7D76CF1F02E"},
        {"70ED9007F287FF4B45F6051C7B41F930ECBBD3672A1ADAF5E749F9AC63058CF7", "110072220032000025007088263700000000000000003800000000000000005587DCD6E60C94FBBEB0D191AF74F7DAB43BE850F5A2900058F632707A469AFDD86280000000000000000000000000000000000000005553440000000000000000000000000000000000000000000000000166800000000000000000000000000000000000000055534400000000000A20B3C85F482532A9578DBB3950B85CA06594D167D6A386F26F2869800000000000000000000000005553440000000000A7D0DCECA71AA25C4CAC47AEB3C55B9D1555C7E0"},
        {"74FF441C31DD16ABF769358E987922B42164D8FEAD7AF4B887CE91E362354B9D", "110072220031000025009BF5AB370000000000000000380000000000000000555453A1569CA9FFC205869B4519E84BFF65D2FB11E9D2B8E7B94347AB0CB045956280000000000000000000000000000000000000005553440000000000000000000000000000000000000000000000000166D6A386F26F2869800000000000000000000000005553440000000000091EACD160A77CFF755309900170B608D964896967800000000000000000000000000000000000000055534400000000000A20B3C85F482532A9578DBB3950B85CA06594D1"},
        {"89A6AC29B8A555E301E60E0F776D3F4D702AD673D971B2DAC40A606E4518F2E4", "110072220032000025007FC5C837000000000000000038000000000000000055472B8DF4924BD51C7D6D747488EB98515894D302F0996119F2240855A8EA68586280000000000000000000000000000000000000005553440000000000000000000000000000000000000000000000000166800000000000000000000000000000000000000055534400000000000A20B3C85F482532A9578DBB3950B85CA06594D167D6A386F26F286980000000000000000000000000555344000000000071D3000DB576F7F41296A7FD4F1E4E81251AEB46"},
        {"8DE7A057F04DE9FAF8B39D5F41A9C214A71FC49ECFFEBA0A4AE41D636D4B9731", "110072220032000025007FC5C837000000000000000038000000000000000055472B8DF4924BD51C7D6D747488EB98515894D302F0996119F2240855A8EA68586280000000000000000000000000000000000000005553440000000000000000000000000000000000000000000000000166800000000000000000000000000000000000000055534400000000000A20B3C85F482532A9578DBB3950B85CA06594D167D6A386F26F286980000000000000000000000000555344000000000071D3000DB576F7F41296A7FD4F1E4E81251AEB46"},
        {"8F338D24CEC539BA6BD046C91B298C155E4A6751339EFFDD6DB75A9D2C912056", "110072220032000025007FC5C837000000000000000038000000000000000055472B8DF4924BD51C7D6D747488EB98515894D302F0996119F2240855A8EA68586280000000000000000000000000000000000000005553440000000000000000000000000000000000000000000000000166800000000000000000000000000000000000000055534400000000000A20B3C85F482532A9578DBB3950B85CA06594D167D6A386F26F286980000000000000000000000000555344000000000071D3000DB576F7F41296A7FD4F1E4E81251AEB46"},
        {"AB7FF2443BCB33302AF0AED781518161C02493D6C77AAF3326B40FB644026C67", "110072220032000025007FC5C837000000000000000038000000000000000055472B8DF4924BD51C7D6D747488EB98515894D302F0996119F2240855A8EA68586280000000000000000000000000000000000000005553440000000000000000000000000000000000000000000000000166800000000000000000000000000000000000000055534400000000000A20B3C85F482532A9578DBB3950B85CA06594D167D6A386F26F286980000000000000000000000000555344000000000071D3000DB576F7F41296A7FD4F1E4E81251AEB46"},
        {"AC09A2D372B0D82B63C29A8237AA8DAC6A39C23C18EB564D0BF008598B2B7056", "110072220032000025007FC5C837000000000000000038000000000000000055472B8DF4924BD51C7D6D747488EB98515894D302F0996119F2240855A8EA68586280000000000000000000000000000000000000005553440000000000000000000000000000000000000000000000000166800000000000000000000000000000000000000055534400000000000A20B3C85F482532A9578DBB3950B85CA06594D167D6A386F26F286980000000000000000000000000555344000000000071D3000DB576F7F41296A7FD4F1E4E81251AEB46"},
        {"B21217716EB5F7A7264114DCD9B64B4625B7C063C02B791829CC8EA2F73C1EC6", "110072220032000025007FC5C837000000000000000038000000000000000055472B8DF4924BD51C7D6D747488EB98515894D302F0996119F2240855A8EA68586280000000000000000000000000000000000000005553440000000000000000000000000000000000000000000000000166800000000000000000000000000000000000000055534400000000000A20B3C85F482532A9578DBB3950B85CA06594D167D6A386F26F286980000000000000000000000000555344000000000071D3000DB576F7F41296A7FD4F1E4E81251AEB46"},
        {"CFD2EF4D26701D78CD0F973D11AFAE54D7776FE3AC771A9EF496311B0DFF9CC7", "110072220032000025007FC5C837000000000000000038000000000000000055472B8DF4924BD51C7D6D747488EB98515894D302F0996119F2240855A8EA68586280000000000000000000000000000000000000005553440000000000000000000000000000000000000000000000000166800000000000000000000000000000000000000055534400000000000A20B3C85F482532A9578DBB3950B85CA06594D167D6A386F26F286980000000000000000000000000555344000000000071D3000DB576F7F41296A7FD4F1E4E81251AEB46"},
        {"E2F8B72F157DE5E4BD4A0B53B15E7402CB0DFE285308C209122D4B4B05CE4534", "110072220032000025007FC5C837000000000000000038000000000000000055472B8DF4924BD51C7D6D747488EB98515894D302F0996119F2240855A8EA68586280000000000000000000000000000000000000005553440000000000000000000000000000000000000000000000000166800000000000000000000000000000000000000055534400000000000A20B3C85F482532A9578DBB3950B85CA06594D167D6A386F26F286980000000000000000000000000555344000000000071D3000DB576F7F41296A7FD4F1E4E81251AEB46"},
        {"F1C0F8517663FD4F0DE1E244EF530C0F67F097B04E37B86BFE93C9A8406CE085", "110072220032000025007FC5C837000000000000000038000000000000000055472B8DF4924BD51C7D6D747488EB98515894D302F0996119F2240855A8EA68586280000000000000000000000000000000000000005553440000000000000000000000000000000000000000000000000166800000000000000000000000000000000000000055534400000000000A20B3C85F482532A9578DBB3950B85CA06594D167D6A386F26F286980000000000000000000000000555344000000000071D3000DB576F7F41296A7FD4F1E4E81251AEB46"},
        {"F619374F8E352A523B99BD10E0555C0724A958D48BBC8F58B232A8BBBFD87629", "110072220032000025007FC5C837000000000000000038000000000000000055472B8DF4924BD51C7D6D747488EB98515894D302F0996119F2240855A8EA68586280000000000000000000000000000000000000005553440000000000000000000000000000000000000000000000000166800000000000000000000000000000000000000055534400000000000A20B3C85F482532A9578DBB3950B85CA06594D167D6A386F26F286980000000000000000000000000555344000000000071D3000DB576F7F41296A7FD4F1E4E81251AEB46"},
        {"F9E0F01515D9FDE99FB77AE4CB73D819079F9D41D43A6483A52E981DE8DE144B", "110072220032000025007FC5C837000000000000000038000000000000000055472B8DF4924BD51C7D6D747488EB98515894D302F0996119F2240855A8EA68586280000000000000000000000000000000000000005553440000000000000000000000000000000000000000000000000166800000000000000000000000000000000000000055534400000000000A20B3C85F482532A9578DBB3950B85CA06594D167D6A386F26F286980000000000000000000000000555344000000000071D3000DB576F7F41296A7FD4F1E4E81251AEB46"},
        {"FF0669FD99C7E22F11B6F14EBE1A6C0D311AC7E681F8CDFD0BB3C7ACA7E8FD38", "110072220032000025007FC5C837000000000000000038000000000000000055472B8DF4924BD51C7D6D747488EB98515894D302F0996119F2240855A8EA68586280000000000000000000000000000000000000005553440000000000000000000000000000000000000000000000000166800000000000000000000000000000000000000055534400000000000A20B3C85F482532A9578DBB3950B85CA06594D167D6A386F26F286980000000000000000000000000555344000000000071D3000DB576F7F41296A7FD4F1E4E81251AEB46"}
    };

    std::string rawHeader =
        "03C3141A01633CD656F91B4EBB5EB89B791BD34DBC8A04BB6F407C5335"
        "BC54351E"
        "DD73"
        "3898497E809E04074D14D271E4832D7888754F9230800761563A292FA2"
        "315A6DB6"
        "FE30"
        "CC5909B285080FCD6773CC883F9FE0EE4D439340AC592AADB973ED3CF5"
        "3E2232B3"
        "3EF5"
        "7CECAC2816E3122816E31A0A00F8377CD95DFA484CFAE282656A58CE5A"
        "A29652EF"
        "FD80"
        "AC59CD91416E4E13DBBE";

    auto hexStringToBinaryString = [](auto const& hex) {
        auto blob = ripple::strUnHex(hex);
        std::string strBlob;
        for (auto c : *blob)
        {
            strBlob += c;
        }
        return strBlob;
    };
    auto binaryStringToUint256 = [](auto const& bin) -> ripple::uint256 {
        ripple::uint256 uint;
        return uint.fromVoid((void const*)bin.data());
    };
    auto ledgerInfoToBinaryString = [](auto const& info) {
        auto blob = RPC::ledgerInfoToBlob(info, true);
        std::string strBlob;
        for (auto c : blob)
        {
            strBlob += c;
        }
        return strBlob;
    };

    std::string rawHeaderBlob = hexStringToBinaryString(rawHeader);
    ripple::LedgerInfo lgrInfo =
        deserializeHeader(ripple::makeSlice(rawHeaderBlob));

    backend.startWrites();
    backend.writeLedger(lgrInfo, std::move(rawHeaderBlob));
    backend.writeSuccessor(
        uint256ToString(Backend::firstKey),
        lgrInfo.seq,
        uint256ToString(Backend::lastKey));
    ASSERT_TRUE(backend.finishWrites(lgrInfo.seq));
    {
        auto rng = backend.fetchLedgerRange();
        EXPECT_TRUE(rng.has_value());
        EXPECT_EQ(rng->minSequence, rng->maxSequence);
        EXPECT_EQ(rng->maxSequence, lgrInfo.seq);
    }
    {
        auto seq = backend.fetchLatestLedgerSequence(yield);
        EXPECT_TRUE(seq.has_value());
        EXPECT_EQ(*seq, lgrInfo.seq);
    }

    {
        auto retLgr = backend.fetchLedgerBySequence(lgrInfo.seq, yield);
        ASSERT_TRUE(retLgr.has_value());
        EXPECT_EQ(retLgr->seq, lgrInfo.seq);
        EXPECT_EQ(
            RPC::ledgerInfoToBlob(lgrInfo), RPC::ledgerInfoToBlob(*retLgr));
    }

    EXPECT_FALSE(
        backend.fetchLedgerBySequence(lgrInfo.seq + 1, yield).has_value());
    auto lgrInfoOld = lgrInfo;

    auto lgrInfoNext = lgrInfo;
    lgrInfoNext.seq = lgrInfo.seq + 1;
    lgrInfoNext.parentHash = lgrInfo.hash;
    lgrInfoNext.hash++;
    lgrInfoNext.accountHash = ~lgrInfo.accountHash;
    {
        std::string rawHeaderBlob = ledgerInfoToBinaryString(lgrInfoNext);

        backend.startWrites();
        backend.writeLedger(lgrInfoNext, std::move(rawHeaderBlob));
        ASSERT_TRUE(backend.finishWrites(lgrInfoNext.seq));
    }
    {
        auto rng = backend.fetchLedgerRange();
        EXPECT_TRUE(rng.has_value());
        EXPECT_EQ(rng->minSequence, lgrInfoOld.seq);
        EXPECT_EQ(rng->maxSequence, lgrInfoNext.seq);
    }
    {
        auto seq = backend.fetchLatestLedgerSequence(yield);
        EXPECT_EQ(seq, lgrInfoNext.seq);
    }
    {
        auto retLgr = backend.fetchLedgerBySequence(lgrInfoNext.seq, yield);
        EXPECT_TRUE(retLgr.has_value());
        EXPECT_EQ(retLgr->seq, lgrInfoNext.seq);
        EXPECT_EQ(
            RPC::ledgerInfoToBlob(*retLgr), RPC::ledgerInfoToBlob(lgrInfoNext));
        EXPECT_NE(
            RPC::ledgerInfoToBlob(*retLgr), RPC::ledgerInfoToBlob(lgrInfoOld));
        retLgr = backend.fetchLedgerBySequence(lgrInfoNext.seq - 1, yield);
        EXPECT_EQ(
            RPC::ledgerInfoToBlob(*retLgr), RPC::ledgerInfoToBlob(lgrInfoOld));
        EXPECT_NE(
            RPC::ledgerInfoToBlob(*retLgr), RPC::ledgerInfoToBlob(lgrInfoNext));
        retLgr = backend.fetchLedgerBySequence(lgrInfoNext.seq - 2, yield);
        EXPECT_FALSE(backend.fetchLedgerBySequence(lgrInfoNext.seq - 2, yield)
                         .has_value());

        auto txns =
            backend.fetchAllTransactionsInLedger(lgrInfoNext.seq, yield);
        EXPECT_EQ(txns.size(), 0);

        auto hashes =
            backend.fetchAllTransactionHashesInLedger(lgrInfoNext.seq, yield);
        EXPECT_EQ(hashes.size(), 0);
    }

    {
        backend.startWrites();
        lgrInfoNext.seq = lgrInfoNext.seq + 1;
        lgrInfoNext.txHash = ~lgrInfo.txHash;
        lgrInfoNext.accountHash = lgrInfoNext.accountHash ^ lgrInfoNext.txHash;
        lgrInfoNext.parentHash = lgrInfoNext.hash;
        lgrInfoNext.hash++;
          
        backend.writeLedger(lgrInfoNext, ledgerInfoToBinaryString(lgrInfoNext));

        backend.writeLedgerObject(
            hexStringToBinaryString(accountIndex),
            lgrInfoNext.seq,
            hexStringToBinaryString(account));

        for (auto const& [keyHex, objHex] : accountHexBlobs)
        {
            backend.writeLedgerObject(
                hexStringToBinaryString(keyHex),
                lgrInfoNext.seq,
                hexStringToBinaryString(objHex));
        }

        ASSERT_TRUE(backend.finishWrites(lgrInfoNext.seq));
    }


}

TYPED_TEST_SUITE(Clio, cfgMOCK);

TYPED_TEST(Clio, accountCurrencies)
{
    boost::asio::io_context ioc;
    std::optional<boost::asio::io_context::work> work;
    work.emplace(ioc);
    std::atomic_bool done = false;

    boost::asio::spawn(
        ioc, [this, &done, &work](boost::asio::yield_context yield) {
            boost::log::core::get()->set_filter(
                boost::log::trivial::severity >= boost::log::trivial::warning);

            std::string keyspace = this->keyspace();

            auto session = std::make_shared<MockSubscriber>();
            Backend::LedgerRange range;
            range.maxSequence = 63116316;
            range.minSequence = 63116310;

            writeAccountCurrencies(this->app().backend(), yield);

            boost::json::object request = {
                {"method", "account_currencies"},
                {"account", "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"},
                {"ledger_index", 63116316},
                {"limit", 10}};

            auto context = RPC::make_WsContext(
                request, this->app(), session, range, "127.0.0.1", yield);

            ASSERT_TRUE(context);

            auto result = RPC::buildResponse(*context);

            ASSERT_TRUE(std::holds_alternative<boost::json::object>(result));

            auto obj = std::get<boost::json::object>(result);

            ASSERT_EQ(obj["send_currencies"].as_array().size(), 2);
            ASSERT_EQ(obj["receive_currencies"].as_array().size(), 0);

            done = true;
            work.reset();
        });

    ioc.run();
    EXPECT_EQ(done, true);
}