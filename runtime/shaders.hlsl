//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float3 worldPos : TEXCOORD1;
};

struct MVPTransforms
{
    matrix model;
    matrix view;
    matrix projection;
    matrix viewProjection;
};

ConstantBuffer<MVPTransforms> MVP_CB : register(b0);

PSInput VSMain(float3 position : POSITION, float4 color : COLOR)
{
    PSInput result;
    result.worldPos = mul(MVP_CB.model, float4(position, 1.0f)).xyz;
    result.position = mul(MVP_CB.viewProjection, float4(result.worldPos, 1.0f));
    result.color = color;

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return input.color;
}
