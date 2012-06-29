-module(nefs).

-include_lib("kernel/include/file.hrl").

-behavior(gen_server).

-export([init/1, handle_call/3, handle_cast/2, handle_info/2, terminate/2, code_change/3]).

init([Base])->
	{ok,state}.

handle_call({getattr,Path},_,Base)->
	RealPath = Base++Path,
	io:format("~n getattr : \"~s\"",[RealPath]),
	case file:read_file_info(RealPath) of
		{ok,#file_info{
			mode = Mode,
			type = directory,
			links = NLinks
			}}} -> {reply,{Type,Mode,NLinks},Base};
		{ok,#file_info{
			mode = Mode,
			type = Type,
			links = NLinks,
			size = Size
			}}} -> {reply,{Type,Mode,NLinks,Size},Base};
	end;

handle_call({open,Path,Flags},_,Base)->
	io:format("~n open : \"~s\"",[Path]),
	case Path of
		"/" -> {reply,{directory,0755,2},Base};
		"/hello" -> {reply,{file,0444,1,13},Base}
	end;
handle_call(_,_,Base)->
	{reply,{error,invalid_command},Base}.

handle_cast(_,State)->
	{noreply,State}.

handle_info(Msg,State)->
	io:format("~nReceived Message ~w",[Msg]),
	{noreply,State}.

terminate(_,_)->
	ok.

code_change(_,_,State)->
	{ok,State}.

